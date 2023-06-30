package didagle

import (
	"context"
	"fmt"
	"reflect"
	"runtime/debug"
	"sync"
	"sync/atomic"
	"time"
	"unsafe"

	mapset "github.com/deckarep/golang-set/v2"
)

const (
	PhaseExec     = 0
	PhasePreExec  = 1
	PhasePostExec = 2

	ExecOk      = 0
	ExecFailed  = 1
	ExecTimeout = 2
)

// A header for a Go map.
type hmap struct {
	// Note: the format of the hmap is also encoded in cmd/compile/internal/reflectdata/reflect.go.
	// Make sure this stays in sync with the compiler's definition.
	count     int // # live cells == size of map.  Must be first (used by len() builtin)
	flags     uint8
	B         uint8  // log_2 of # of buckets (can hold up to loadFactor * 2^B items)
	noverflow uint16 // approximate number of overflow buckets; see incrnoverflow for details
	hash0     uint32 // hash seed

	buckets    unsafe.Pointer // array of 2^B Buckets. may be nil if count==0.
	oldbuckets unsafe.Pointer // previous bucket array of half the size, non-nil only when growing
	nevacuate  uintptr        // progress counter for evacuation (buckets less than this have been evacuated)

	extra unsafe.Pointer // optional fields
}

// DagEvent dag事件
type DagEvent struct {
	Operator string
	Graph    string
	Phase    int
	Start    time.Time
	End      time.Time
	Code     int
}

var eventCollector chan *DagEvent

func collectEvent(event *DagEvent) {
	if nil != eventCollector {
		select {
		case eventCollector <- event:
		default:
			// message dropped
		}
	}
}

type CondParamsContext struct {
	params       *Params
	evalOp       *OperatorInstance
	expectConfig string
}

type VertexExecuteContext struct {
	Vertex   *Vertex
	childIdx int
	graphCtx *GraphExecuteContext

	opInstance    *OperatorInstance
	operator      *OperatorRegistry
	subgraphCtx   *GraphClusterExecuteContext
	successors    []*VertexExecuteContext
	depsResults   []int
	waitingNum    int32
	childGraphIdx int
	params        *Params
	condParams    []*CondParamsContext

	expectEvalOp *OperatorInstance
}

func (v *VertexExecuteContext) ready() bool {
	return v.waitingNum == 0
}

func (v *VertexExecuteContext) build() error {
	if len(v.Vertex.Operator) > 0 {
		var err error
		v.operator, err = GetOperator(v.Vertex.Operator)
		if nil != err {
			return err
		}
		opInstance := v.operator.GetOperator()
		args := v.Vertex.Args
		if len(v.Vertex.While) > 0 {
			if nil == args {
				args = make(map[string]interface{})
			}
			args[ExprParamKey] = v.Vertex.While
			args[WhileExecCluterParamKey] = v.Vertex.Cluster
			args[WhileExecGraphParamKey] = v.Vertex.Graph
			args[WhileAsyncExecParamKey] = v.Vertex.Async
		}
		err = opInstance.op.Setup(args)
		if nil != err {
			return err
		}
		v.opInstance = opInstance
	}

	for vid := range v.Vertex.successorVertex {
		vertexCtx := v.graphCtx.getVertexContext(vid)
		if nil == vertexCtx {
			DefaultLogger.Errorf("No vertex found with id:%v", vid)
			return ErrVertexNotFound
		}
		v.successors = append(v.successors, vertexCtx)
	}
	v.waitingNum = int32(len(v.Vertex.depsExpectResults))
	v.depsResults = make([]int, v.waitingNum)
	v.params = NewParams(v.Vertex.Args)
	for _, cond := range v.Vertex.SelectArgs {
		if cond.IsCondExpr() {
			evalOpReg := v.Vertex.g.cluster.defaultExprOperator
			evalOp := evalOpReg.GetOperator()
			args := make(map[string]interface{})
			args[ExprParamKey] = cond.Match
			err := evalOp.op.Setup(args)
			if nil != err {
				return err
			}
			params := NewParams(cond.Args)
			v.condParams = append(v.condParams, &CondParamsContext{
				params: params,
				evalOp: evalOp,
			})
		} else {
			params := NewParams(cond.Args)
			v.condParams = append(v.condParams, &CondParamsContext{
				params:       params,
				expectConfig: cond.Match,
			})
		}
	}
	if len(v.Vertex.Expect) > 0 {
		evalOpReg := v.Vertex.g.cluster.defaultExprOperator
		evalOp := evalOpReg.GetOperator()
		args := make(map[string]interface{})
		args[ExprParamKey] = v.Vertex.Expect
		err := evalOp.op.Setup(args)
		if nil != err {
			return err
		}
		v.expectEvalOp = evalOp
	}
	return nil
}

func (v *VertexExecuteContext) Reset() {
	if nil != v.params {
		v.params.reset()
	}
	if nil != v.subgraphCtx {
		v.subgraphCtx.recycle()
		v.subgraphCtx = nil
	}
	if nil != v.opInstance {
		v.operator.resetInputs(v.opInstance)
		v.operator.resetOutputs(v.opInstance)
		v.opInstance.op.Reset()
	}
	for _, cond := range v.condParams {
		cond.params.reset()
	}
	v.waitingNum = int32(len(v.Vertex.depsExpectResults))
	for i := range v.depsResults {
		v.depsResults[i] = 0
	}
}

func (v *VertexExecuteContext) setDependencyResult(dep *VertexExecuteContext, result int) int32 {
	idx := v.Vertex.depsVertexIdxMap[dep.Vertex.ID]
	v.depsResults[idx] = result
	return atomic.AddInt32(&v.waitingNum, -1)
}

func (v *VertexExecuteContext) injectParams(params *Params) error {
	if nil == v.opInstance {
		return nil
	}
	for _, param := range v.operator.Params {
		fieldPtr := v.opInstance.ptr + param.Offset
		val, exist := params.Get(param.Name)
		if !exist {
			continue
		}
		switch param.Type.Kind() {
		case reflect.Bool:
			bv, ok := val.(bool)
			if ok {
				*(*bool)(unsafe.Pointer(fieldPtr)) = bv
			}

		case reflect.String:
			bv, ok := val.(string)
			if ok {
				*(*string)(unsafe.Pointer(fieldPtr)) = bv
			}

		case reflect.Int:
			bv, ok := val.(int)
			if ok {
				*(*int)(unsafe.Pointer(fieldPtr)) = bv
			}

		case reflect.Int32:
			bv, ok := val.(int32)
			if ok {
				*(*int32)(unsafe.Pointer(fieldPtr)) = bv
			}

		case reflect.Int64:
			bv, ok := val.(int64)
			if ok {
				*(*int64)(unsafe.Pointer(fieldPtr)) = bv
			}

		case reflect.Float64:
			bv, ok := val.(float64)
			if ok {
				*(*float64)(unsafe.Pointer(fieldPtr)) = bv
			}

		case reflect.Float32:
			bv, ok := val.(float32)
			if ok {
				*(*float32)(unsafe.Pointer(fieldPtr)) = bv
			}
		case reflect.Slice:
			ifa := val.([]interface{})
			switch param.OrigInputType.Kind() {
			case reflect.Bool:
				tmp := []bool{}
				for _, v := range ifa {
					tmp = append(tmp, v.(bool))
				}
				*(*[]bool)(unsafe.Pointer(fieldPtr)) = tmp

			case reflect.String:
				tmp := []string{}
				for _, v := range ifa {
					tmp = append(tmp, v.(string))
				}
				*(*[]string)(unsafe.Pointer(fieldPtr)) = tmp

			case reflect.Int:
				tmp := []int{}
				for _, v := range ifa {
					t := v.(int64)
					tmp = append(tmp, int(t))
				}
				*(*[]int)(unsafe.Pointer(fieldPtr)) = tmp

			case reflect.Int32:
				tmp := []int32{}
				for _, v := range ifa {
					t := v.(int64)
					tmp = append(tmp, int32(t))
				}
				*(*[]int32)(unsafe.Pointer(fieldPtr)) = tmp

			case reflect.Int64:
				tmp := []int64{}
				for _, v := range ifa {
					t := v.(int64)
					tmp = append(tmp, int64(t))
				}
				*(*[]int64)(unsafe.Pointer(fieldPtr)) = tmp

			case reflect.Float64:
				tmp := []float64{}
				for _, v := range ifa {
					t := v.(float64)
					tmp = append(tmp, float64(t))
				}
				*(*[]float64)(unsafe.Pointer(fieldPtr)) = tmp

			case reflect.Float32:
				tmp := []float32{}
				for _, v := range ifa {
					t := v.(float64)
					tmp = append(tmp, float32(t))
				}
				*(*[]float32)(unsafe.Pointer(fieldPtr)) = tmp
			}
		}
	}
	return nil
}

func (v *VertexExecuteContext) injectSpan(span Span) error {
	if nil == v.operator.Span || nil == span {
		return nil
	}
	fieldPtr := v.opInstance.ptr + v.operator.Span.Offset
	ptr := (*Iface)(unsafe.Pointer(&span))
	iptr := (*Iface)(unsafe.Pointer(fieldPtr))
	iptr.Data = unsafe.Pointer(ptr.Data)
	iptr.Type = unsafe.Pointer(ptr.Type)
	return nil
}

func (v *VertexExecuteContext) injectContext(context *ExecuteContext) error {
	if nil == v.operator.Context {
		return nil
	}
	fieldPtr := v.opInstance.ptr + v.operator.Context.Offset
	*(*uintptr)(unsafe.Pointer(fieldPtr)) = uintptr(unsafe.Pointer(context))
	return nil
}

func (v *VertexExecuteContext) injectInputs(context *ExecuteContext) error {
	for _, input := range v.Vertex.Input {
		if !input.IsMapInput {
			fieldPtr := v.opInstance.ptr + input.opField.Offset
			id := DataID{
				Name:     input.ID,
				TypeName: input.opField.OrigInputType.Name(),
				TypeKind: input.opField.OrigInputType.Kind(),
			}
			ptr, err := context.getVal(id)
			// DefaultLogger.Debugf("%p inject input data id: %v %v %v %v", context, id, ptr, input.opField.Type.Kind(), err)
			if nil == err {
				switch input.opField.Type.Kind() {
				case reflect.Interface:
					iptr := (*Iface)(unsafe.Pointer(fieldPtr))
					iptr.Data = unsafe.Pointer(ptr.Data)
					iptr.Type = unsafe.Pointer(ptr.Type)
				case reflect.Slice:
					fh := (*reflect.SliceHeader)(unsafe.Pointer(fieldPtr))
					sh := (*reflect.SliceHeader)(unsafe.Pointer(ptr.Data))
					*fh = *sh
					// *(*uintptr)(unsafe.Pointer(fieldPtr)) = uintptr(ptr.Data)
				case reflect.Map:
					// fieldPtr is hmap pointer
					*(*uintptr)(unsafe.Pointer(fieldPtr)) = uintptr(ptr.Data)
					// tmp := (*map[string]string)(unsafe.Pointer(fieldPtr))
					// fmt.Printf("####%v\n", tmp)
				case reflect.Pointer:
					*(*uintptr)(unsafe.Pointer(fieldPtr)) = uintptr(ptr.Data)
				default:
					DefaultLogger.Errorf("[%v]Unsupported input field %v/%v", v.Vertex.Operator, input.opField.Name, input.opField.Type)
				}
				if input.Move {
					ptr.Data = nil
				}
			} else {
				DefaultLogger.Errorf("[%v] Not exist for normal input:%v %v", id, err, len(context.dataCtx))
			}
		} else {
			// mapVal := reflect.Indirect(reflect.ValueOf(v.opInstance.op)).FieldByIndex(input.opField.Index)
			fieldPtr := v.opInstance.ptr + input.opField.Offset
			mapVal := (*map[string]unsafe.Pointer)(unsafe.Pointer(fieldPtr))
			// DefaultLogger.Errorf("map len %v", len(*mapVal))
			for _, aggregate := range input.Aggregate {
				id := DataID{
					Name:     aggregate,
					TypeName: input.opField.OrigInputType.Name(),
					TypeKind: input.opField.OrigInputType.Kind(),
				}
				ptr, err := context.getVal(id)
				if nil == err {
					// (*mapVal)[aggregate] = unsafe.Pointer(ptr)
					iv := (*Iface)(unsafe.Pointer(ptr))
					// data := (*string)(str.Data)
					// DefaultLogger.Errorf("Map set %v->%v", aggregate, *data)
					(*mapVal)[aggregate] = iv.Data
					// mapVal.SetMapIndex(reflect.ValueOf(aggregate), reflect.NewAt(input.opField.OrigInputType, str.Data))
				} else {
					DefaultLogger.Errorf("[%v] Not exist for map input", id)
				}
			}
		}
	}
	return nil
}
func (v *VertexExecuteContext) emitOutputs(context *ExecuteContext) error {
	for _, output := range v.Vertex.Output {
		fieldPtr := v.opInstance.ptr + output.opField.Offset
		outType := output.opField.Type
		if output.IsInOut {
			outType = outType.Elem()
		}
		id := DataID{
			Name:     output.ID,
			TypeName: outType.Name(),
			TypeKind: outType.Kind(),
		}
		if output.IsInOut {
			dataPtr := (*uintptr)(unsafe.Pointer(fieldPtr))
			_ = context.set(id, *dataPtr, 0)
		} else {
			switch outType.Kind() {
			case reflect.Slice:
				// p := (*uintptr)(unsafe.Pointer(fieldPtr))
				_ = context.set(id, fieldPtr, 0)
				// tmp := (*[]string)(unsafe.Pointer(fieldPtr))
				// sh := (*reflect.SliceHeader)(unsafe.Pointer(fieldPtr))
				// fmt.Printf("#####id:%v %v %v %v\n", id, p, *tmp, sh)
			case reflect.Map:
				p := (*uintptr)(unsafe.Pointer(fieldPtr))
				_ = context.set(id, *p, 0)
			case reflect.Interface:
				pp := (*Iface)(unsafe.Pointer(fieldPtr))
				_ = context.set(id, uintptr(pp.Data), uintptr(pp.Type))
			default:
				_ = context.set(id, fieldPtr, 0)
			}
		}
	}
	return nil
}
func (v *VertexExecuteContext) execute(ctx context.Context, context *ExecuteContext) error {
	result := 0
	eagerExit := false

	preEvent := &DagEvent{
		Operator: v.Vertex.Operator,
		Graph:    v.Vertex.g.fullName,
		Phase:    PhasePreExec,
		Start:    time.Now(),
	}
	execEvent := &DagEvent{
		Operator: v.Vertex.Operator,
		Graph:    v.Vertex.g.fullName,
		Phase:    PhaseExec,
	}

	for i, vertexResult := range v.depsResults {
		switch v.Vertex.depsExpectResults[i] {
		case ResultAll:
			// nothing
		case ResultOk:
			if vertexResult != ResultOk {
				result = ResultSkip
				eagerExit = true
				break
			}
		case ResultErr:
			if vertexResult != ResultErr {
				result = ResultSkip
				eagerExit = true
				break
			}
		default:
			DefaultLogger.Errorf("return by %v", v.Vertex.depsExpectResults[i])
			return ErrVertexNotFound
		}
	}
	// DefaultLogger.Debugf("Operator:%v eagerExit:%v, expect_deps:%v, deps:%v", v.Vertex.Operator, eagerExit, v.Vertex.depsExpectResults, v.depsResults)
	if !eagerExit {
		if nil != v.expectEvalOp {
			err := v.expectEvalOp.Execute(ctx, v.graphCtx.cluster.runningParams,
				WithCurrentGraph(v.graphCtx.Graph.fullName))
			if nil != err {
				result = ResultSkip
				eagerExit = true
			}
		}
		if len(v.Vertex.ExpectConfig) > 0 {
			expectResult := true
			configName := v.Vertex.ExpectConfig
			logicNot := false
			if v.Vertex.ExpectConfig[0] == '!' {
				configName = configName[1:]
				logicNot = true
			}
			val, exist := v.graphCtx.cluster.runningParams.Get(configName)
			if !exist {
				expectResult = false
			}
			if bv, ok := val.(bool); ok {
				expectResult = bv
				if logicNot {
					expectResult = !bv
				}
			} else {
				expectResult = false
			}
			if !expectResult {
				result = ResultSkip
				eagerExit = true
			}
		}
	}

	if eagerExit {
		v.graphCtx.vertexDone(ctx, v, result)
		return nil
	}
	if nil != v.graphCtx.cluster.runningParams {
		v.params.setParent(v.graphCtx.cluster.runningParams)
	}
	selectParams := v.params
	// Select running params
	for _, cond := range v.condParams {
		if nil != cond.evalOp {
			err := cond.evalOp.Execute(ctx, v.graphCtx.cluster.runningParams,
				WithCurrentGraph(v.graphCtx.Graph.fullName))
			if nil == err {
				selectParams = cond.params
				break
			}
		} else {
			expectResult := true
			configName := cond.expectConfig
			logicNot := false
			if configName[0] == '!' {
				configName = configName[1:]
				logicNot = true
			}
			val, exist := v.graphCtx.cluster.runningParams.Get(configName)
			if !exist {
				expectResult = false
			}
			if bv, ok := val.(bool); ok {
				expectResult = bv
				if logicNot {
					expectResult = !bv
				}
			} else {
				expectResult = false
			}
			if expectResult {
				selectParams = cond.params
				break
			}
		}
	}
	if nil != v.graphCtx.cluster.runningParams {
		selectParams.setParent(v.graphCtx.cluster.runningParams)
	}

	defer func() {
		if r := recover(); r != nil {
			v.graphCtx.vertexDone(ctx, v, ResultErr)
			DefaultLogger.Errorf("Vertex:%v execute error:%v, stack:\n%v", v.Vertex.ID, r, string(debug.Stack()))
		}
	}()

	var execErr error
	if nil != v.opInstance {
		var opSpan Span
		if nil != context.Span {
			opSpan = context.Span.NewChild(v.Vertex.Operator)
		}
		_ = v.injectContext(context)
		_ = v.injectSpan(opSpan)
		_ = v.injectInputs(context)
		_ = v.injectParams(selectParams)
		preEvent.End = time.Now()
		execEvent.Start = preEvent.End

		execErr = v.opInstance.Execute(ctx, selectParams, WithCurrentGraph(v.graphCtx.Graph.fullName))

		if nil != opSpan {
			if nil != execErr {
				opSpan.SetAttribute("err", execErr)
			}
			opSpan.End()
		}
		DefaultLogger.Debugf("Operator:%v execute with rc:%v", v.Vertex.Operator, execErr)
	} else if len(v.Vertex.Cluster) > 0 {
		cluster := getCluster(v.Vertex.Cluster)
		if nil == cluster {
			DefaultLogger.Errorf("No cluster found for %v", v.Vertex.Cluster)
			return ErrGraphNotFound
		}
		v.subgraphCtx, _ = cluster.contextPool.Get().(*GraphClusterExecuteContext)
		preEvent.End = time.Now()
		execEvent.Start = preEvent.End
		execErr = v.subgraphCtx.execute(ctx, context, v.Vertex.Graph, selectParams, v.childGraphIdx)
	}
	execEvent.End = time.Now()
	if nil != execErr {
		result = ResultErr
		execEvent.Code = ExecFailed
	} else {
		result = ResultOk
		_ = v.emitOutputs(context)
	}

	// only collect event for operator
	if len(v.Vertex.Operator) > 0 {
		collectEvent(preEvent)
		collectEvent(execEvent)
	}

	v.graphCtx.vertexDone(ctx, v, result)
	return nil
}

type GraphExecuteContext struct {
	Graph *Graph

	cluster         *GraphClusterExecuteContext
	runningCtx      *ExecuteContext
	outputDataIDs   []DataID
	inputDataIDs    []DataID
	vertices        map[string]*VertexExecuteContext
	pendingVertices int32
	subgraphNum     int
}

func (g *GraphExecuteContext) vertexDone(ctx context.Context, v *VertexExecuteContext, result int) {
	if result == ResultSkip {
		result = ResultErr
	}
	if atomic.AddInt32(&g.pendingVertices, -1) == 0 {
		g.cluster.wg.Done()
		return
	}
	// var readyVertices []*VertexExecuteContext
	for _, successor := range v.successors {
		// DefaultLogger.Debugf("Operator:%v successor:%v wait:%v %v ", v.Vertex.Operator, successor.Vertex.Operator, successor.waitingNum, len(successor.Vertex.depsVertexIdxMap))
		rest := successor.setDependencyResult(v, result)
		// DefaultLogger.Debugf("Operator:%v successor:%v rest:%v", v.Vertex.Operator, successor.Vertex.Operator, rest)
		if rest == 0 {
			//readyVertices = append(readyVertices, successor)
			readyVertex := successor
			go func() {
				_ = readyVertex.execute(ctx, g.runningCtx)
			}()
		}
	}
	// if len(readyVertices) > 0 {
	// 	g.executeReadyVertices(g.runningCtx, readyVertices, false)
	// }
}

func (g *GraphExecuteContext) getVertexContext(id string) *VertexExecuteContext {
	ctx, exist := g.vertices[id]
	if exist {
		return ctx
	}
	return nil
}

func (g *GraphExecuteContext) build() error {
	g.pendingVertices = int32(len(g.Graph.Vertex))
	for i := range g.Graph.Vertex {
		vertexCtx := &VertexExecuteContext{
			Vertex:   g.Graph.Vertex[i],
			childIdx: i,
			graphCtx: g,
		}
		g.vertices[vertexCtx.Vertex.ID] = vertexCtx
	}
	childGraphIdx := 0
	for _, vertexCtx := range g.vertices {
		if len(vertexCtx.Vertex.Graph) > 0 && len(vertexCtx.Vertex.While) == 0 {
			g.subgraphNum++
			vertexCtx.childGraphIdx = childGraphIdx
			childGraphIdx++
		} else {
			vertexCtx.childGraphIdx = -1
		}

		err := vertexCtx.build()
		if nil != err {
			return err
		}

		for _, input := range vertexCtx.Vertex.Input {
			id := DataID{
				Name:     input.ID,
				TypeName: input.opField.OrigInputType.Name(),
				TypeKind: input.opField.OrigInputType.Kind(),
			}
			g.inputDataIDs = append(g.inputDataIDs, id)
		}
		for _, output := range vertexCtx.Vertex.Output {
			id := DataID{
				Name:     output.ID,
				TypeName: output.opField.Type.Name(),
				TypeKind: output.opField.Type.Kind(),
			}
			g.outputDataIDs = append(g.outputDataIDs, id)
		}
	}
	return nil
}

func (g *GraphExecuteContext) Reset() {
	g.pendingVertices = int32(len(g.Graph.Vertex))
	for _, vertexCtx := range g.vertices {
		vertexCtx.Reset()
	}
}

func (g *GraphExecuteContext) executeReadyVertices(ctx context.Context, exeCtx *ExecuteContext,
	vertices []*VertexExecuteContext, tryInplace bool) error {
	if tryInplace {
		for i := 1; i < len(vertices); i++ {
			go vertices[i].execute(ctx, exeCtx)
		}
	} else {
		for i := 0; i < len(vertices); i++ {
			go vertices[i].execute(ctx, exeCtx)
		}
	}
	if tryInplace {
		_ = vertices[0].execute(ctx, exeCtx)
	}
	// vertices[0].execute(ctx)
	return nil
}
func (g *GraphExecuteContext) execute(ctx context.Context, exeCtx *ExecuteContext) error {
	g.runningCtx = exeCtx
	var readyVertices []*VertexExecuteContext
	for _, vertex := range g.vertices {
		if vertex.ready() {
			readyVertices = append(readyVertices, vertex)
		}
	}
	_ = g.executeReadyVertices(ctx, exeCtx, readyVertices, false)
	return nil
}

type GraphClusterExecuteContext struct {
	Cluster *GraphCluster

	runningGraphCtx *GraphExecuteContext
	runningCtx      *ExecuteContext
	runningParams   *Params
	wg              *sync.WaitGroup
	graphs          map[string]*GraphExecuteContext
	configSettings  []*OperatorInstance
}

func newGraphClusterExecuteContext(c *GraphCluster) *GraphClusterExecuteContext {
	return &GraphClusterExecuteContext{
		Cluster:    c,
		graphs:     make(map[string]*GraphExecuteContext),
		runningCtx: NewExecuteContext(nil),
		wg:         &sync.WaitGroup{},
	}
}

func (c *GraphClusterExecuteContext) Reset() {
	if nil != c.runningGraphCtx {
		c.runningGraphCtx.Reset()
		c.runningGraphCtx = nil
	}
	c.runningCtx.Reset()
	c.wg.Add(1)
}

func (c *GraphClusterExecuteContext) build() error {
	for name, graph := range c.Cluster.graphMap {
		graphCtx := &GraphExecuteContext{
			Graph:    graph,
			cluster:  c,
			vertices: make(map[string]*VertexExecuteContext),
		}
		c.graphs[name] = graphCtx
	}

	for _, graphCtx := range c.graphs {
		err := graphCtx.build()
		if nil != err {

			return err
		}
	}
	for _, config := range c.Cluster.ConfigSetting {
		evalOpReg := c.Cluster.defaultExprOperator
		evalOp := evalOpReg.GetOperator()
		args := make(map[string]interface{})
		args[ExprParamKey] = config.Cond
		err := evalOp.op.Setup(args)
		if nil != err {
			return err
		}
		c.configSettings = append(c.configSettings, evalOp)
	}

	return nil
}

func (c *GraphClusterExecuteContext) recycle() {
	// DefaultLogger.Debugf("Recycle cluster context:%v", c.Cluster.name)
	// go func() {
	c.Reset()
	c.Cluster.contextPool.Put(c)

	// }()
}

func (c *GraphClusterExecuteContext) fillDataEntry(graph string) error {
	g, exist := c.graphs[graph]
	if !exist {
		return ErrGraphNotFound
	}
	for _, id := range g.outputDataIDs {
		c.runningCtx.fillEntry(id)
	}
	return nil
}

func (c *GraphClusterExecuteContext) execute(ctx context.Context, exeCtx *ExecuteContext,
	graph string, params *Params, childIdx int) error {
	if childIdx < 0 {
		defer c.recycle()
	}
	g, exist := c.graphs[graph]
	if !exist {
		return ErrGraphNotFound
	}
	execEvent := &DagEvent{
		Graph: g.Graph.fullName,
		Phase: PhaseExec,
		Start: time.Now(),
	}

	c.runningGraphCtx = g
	_ = c.runningCtx.setParent(exeCtx)

	createChildSpan := false
	if nil != exeCtx && nil != exeCtx.Span {
		c.runningCtx.Span = exeCtx.Span.NewChild(g.Graph.fullName)
		createChildSpan = true
		loopIdx, exist := params.Get("loopIdx")
		if exist {
			c.runningCtx.Span.SetAttribute("LoopIdx", loopIdx)
		}
	}

	if childIdx >= 0 {
		exeCtx.Children[childIdx] = c.runningCtx
	}
	for _, id := range g.outputDataIDs {
		c.runningCtx.fillEntry(id)
	}
	c.runningCtx.reserveChildrenContext(g.subgraphNum)

	if nil == params {
		params = NewParams(make(map[string]interface{}))
	}
	c.runningParams = params

	for i, eval := range c.configSettings {
		result := true
		err := eval.Execute(ctx, params, WithCurrentGraph(c.Cluster.name))
		if nil != err {
			result = false
		}
		params.set(c.Cluster.ConfigSetting[i].Name, result)
	}

	err := g.execute(ctx, c.runningCtx)
	execEvent.End = time.Now()
	if nil != err {
		execEvent.Code = ExecFailed
		c.wg.Done()
	} else {
		c.wg.Wait()
	}
	collectEvent(execEvent)
	if createChildSpan {
		c.runningCtx.Span.End()
	}
	return err
}

type dataValue struct {
	Iface
	Val interface{}
}

type ExecuteContext struct {
	Parent   *ExecuteContext
	Children []*ExecuteContext
	Span     Span

	dataCtx map[DataID]*dataValue
}

type ExecuteContextOptions struct {
	Span   Span
	Parent *ExecuteContext
}

func NewExecuteContext(parent *ExecuteContext) *ExecuteContext {
	return NewExecuteContextWithOptions(&ExecuteContextOptions{
		Parent: parent,
	})
}

func NewExecuteContextWithOptions(option *ExecuteContextOptions) *ExecuteContext {
	return &ExecuteContext{
		dataCtx: make(map[DataID]*dataValue),
		Parent:  option.Parent,
		Span:    option.Span,
	}
}

func (ctx *ExecuteContext) reserveChildrenContext(n int) {
	ctx.Children = make([]*ExecuteContext, n)
}

func (ctx *ExecuteContext) fillEntry(id DataID) {
	existData, exist := ctx.dataCtx[id]
	if exist {
		existData.Data = nil
		existData.Type = nil
		existData.Val = nil
	} else {
		ctx.dataCtx[id] = &dataValue{}
	}

}

func (ctx *ExecuteContext) setParent(p *ExecuteContext) error {
	if nil != ctx.Parent {
		return ErrExistParentContext
	}
	ctx.Parent = p
	return nil
}

func (ctx *ExecuteContext) Reset() {
	ctx.Parent = nil
	if len(ctx.Children) > 0 {
		ctx.Children = make([]*ExecuteContext, 0)
	}
	ctx.dataCtx = make(map[DataID]*dataValue)
	ctx.Span = nil
}

func (ctx *ExecuteContext) get(id DataID, excludes mapset.Set[uintptr], withParent bool,
	withChildren bool) (*dataValue, error) {
	if existData, exist := ctx.dataCtx[id]; exist {
		return existData, nil
	}
	if nil == excludes {
		excludes = mapset.NewThreadUnsafeSet[uintptr]()
	}
	excludes.Add(uintptr(unsafe.Pointer(ctx)))
	if withParent && nil != ctx.Parent {
		if !excludes.Contains(uintptr(unsafe.Pointer(ctx.Parent))) {
			if ptr, err := ctx.Parent.get(id, excludes, true, withChildren); nil == err {
				return ptr, nil
			}
		}
	}
	if withChildren {
		for _, child := range ctx.Children {
			if nil == child {
				continue
			}
			if !excludes.Contains(uintptr(unsafe.Pointer(child))) {
				ptr, err := child.get(id, excludes, false, true)
				if nil == err {
					return ptr, nil
				}
			}
		}
	}
	return nil, ErrDataNotFound

}

func (ctx *ExecuteContext) getVal(id DataID) (*dataValue, error) {
	return ctx.get(id, nil, true, true)
}

func (ctx *ExecuteContext) set(id DataID, data uintptr, typePtr uintptr) error {
	existData, exist := ctx.dataCtx[id]
	if !exist {
		if nil != ctx.Parent {
			return ctx.Parent.set(id, data, typePtr)
		}
		return ErrDataNotFound
	}
	existData.Data = unsafe.Pointer(data)
	existData.Type = unsafe.Pointer(typePtr)
	return nil
}

type Iface struct {
	Type, Data unsafe.Pointer
}

func SetExecuteContext[T any](ctx *ExecuteContext, name string, val T) error {
	origType := reflect.TypeOf((*T)(nil)).Elem()
	elemType := origType

	if elemType.Kind() == reflect.Pointer {
		elemType = elemType.Elem()
	}
	id := DataID{
		Name:     name,
		TypeName: elemType.Name(),
		TypeKind: elemType.Kind(),
	}

	dataVal := &dataValue{}
	dataVal.Val = val
	switch origType.Kind() {
	case reflect.Slice:
		fallthrough
	case reflect.Map:
		// Iface's value is hmap pointer
		fallthrough
	case reflect.Pointer:
		pp := (*Iface)(unsafe.Pointer(&dataVal.Val))
		dataVal.Data = pp.Data
		dataVal.Type = pp.Type
		ctx.dataCtx[id] = dataVal
	case reflect.Interface:
		pp := (*Iface)(unsafe.Pointer(&val))
		dataVal.Data = pp.Data
		dataVal.Type = pp.Type
		ctx.dataCtx[id] = dataVal
	default:
		return fmt.Errorf("Invalid type:%v to inject context", origType)
	}
	return nil

}

func (ctx *ExecuteContext) Set(name string, val interface{}) error {
	return ctx.SetWithType(name, val, reflect.TypeOf(val), nil)
}

// SetWithType 这个丢掉了原有信息
func (ctx *ExecuteContext) SetWithType(name string, val interface{}, typ reflect.Type, iface *Iface) error {
	var t reflect.Type = typ
	// isPointer := false
	if typ.Kind() == reflect.Pointer {
		t = t.Elem()
		// isPointer = true
	}
	id := DataID{
		Name:     name,
		TypeName: t.Name(),
		TypeKind: t.Kind(),
	}
	dataVal := &dataValue{}
	dataVal.Val = val
	switch typ.Kind() {
	case reflect.Pointer:
		pp := (*Iface)(unsafe.Pointer(&dataVal.Val))
		dataVal.Data = pp.Data
		dataVal.Type = pp.Type
		if iface != nil {
			dataVal.Type = iface.Type
		}
		ctx.dataCtx[id] = dataVal
	case reflect.Slice:
		fallthrough
	case reflect.Map:
		// Iface's value is hmap pointer
		fallthrough
	case reflect.Interface:
		pp := (*Iface)(unsafe.Pointer(&dataVal.Val))
		dataVal.Data = pp.Data
		dataVal.Type = pp.Type
		if iface != nil {
			dataVal.Type = iface.Type
		}
		ctx.dataCtx[id] = dataVal
		if reflect.Map == typ.Kind() {
		}
	default:
		return fmt.Errorf("Invalid type:%v to inject context", typ)
	}
	return nil
}

func SetEventCollector(c chan *DagEvent) {
	eventCollector = c
}

func ExecuteGraph(ctx context.Context, cluster string, graph string, exeCtx *ExecuteContext, params *Params) error {
	c := getCluster(cluster)
	if nil == c {
		DefaultLogger.Errorf("No cluster:%v found", cluster)
		return ErrGraphNotFound
	}
	clusterCtx, _ := c.contextPool.Get().(*GraphClusterExecuteContext)
	return clusterCtx.execute(ctx, exeCtx, graph, params, -1)
}
