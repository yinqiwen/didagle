package didagle

import (
	"fmt"
	"log"
	"path/filepath"
	"strings"
	"sync"
	"unicode"

	"github.com/BurntSushi/toml"
)

const (
	ResultOk   = 1
	ResultErr  = 2
	ResultSkip = 3
	ResultAll  = 4
)

var clusterMap sync.Map

func saveCluster(c *GraphCluster) {
	clusterMap.Store(c.name, c)
}

func getCluster(name string) *GraphCluster {
	c, ok := clusterMap.Load(name)
	if ok {
		return c.(*GraphCluster)
	}
	return nil
}

type GraphData struct {
	ID         string   `toml:"id"`
	Field      string   `toml:"field"`
	Aggregate  []string `toml:"aggregate"`
	Cond       string   `toml:"cond"`
	Required   bool     `toml:"required"`
	Move       bool     `toml:"move"`
	IsExtern   bool     `toml:"extern"`
	IsInOut    bool
	IsMapInput bool

	opField *OperatorField
}

type CondParams struct {
	Match string                 `toml:"match"`
	Args  map[string]interface{} `toml:"args"`
}

func (cp *CondParams) IsCondExpr() bool {
	if cp == nil {
		return false
	}
	// 这里本质上是判断只存在标识符（identifier），即regex("_0-9a-zA-Z")，且不存在运算符的情况。vs 表达式
	// "!myVar"是一个特殊用法，也走标识符的逻辑处理
	// TODO(rayrphuang): 仅包含标识符 vs 表达式，应该有更鲁棒的方法去判断
	for _, char := range cp.Match {
		if !(unicode.IsNumber(char) || unicode.IsLetter(char) || char == '!' || char == '_') {
			return true
		}
	}
	return false
}

type Vertex struct {
	ID                  string                 `toml:"id"`
	Operator            string                 `toml:"processor"`
	While               string                 `toml:"while"`
	Cond                string                 `toml:"cond"`
	Expect              string                 `toml:"expect"`
	ExpectConfig        string                 `toml:"expect_config"`
	SelectArgs          []CondParams           `toml:"select_args"`
	Cluster             string                 `toml:"cluster"`
	Graph               string                 `toml:"graph"`
	Successor           []string               `toml:"successor"`
	SuccessorOnOk       []string               `toml:"if"`
	SuccessorOnErr      []string               `toml:"else"`
	SuccessorOnOk1      []string               `toml:"successor_on_ok"`
	SuccessorOnErr1     []string               `toml:"successor_on_err"`
	Deps                []string               `toml:"deps"`
	DepsOnOk            []string               `toml:"deps_on_ok"`
	DepsOnErr           []string               `toml:"deps_on_err"`
	Input               []GraphData            `toml:"input"`
	Output              []GraphData            `toml:"output"`
	Start               bool                   `toml:"start"`
	Async               bool                   `toml:"async"`
	IgnoreOperatorError bool                   `toml:"ignore_processor_execute_error"`
	Args                map[string]interface{} `toml:"args"`

	successorVertex   map[string]*Vertex
	depsVertexIdxMap  map[string]int
	depsExpectResults []int
	isIDGenerated     bool
	isGenerated       bool
	g                 *Graph
}

func (p *Vertex) dumpDotDefine(s *strings.Builder) {
	s.WriteString("    ")
	s.WriteString(p.getDotID())
	s.WriteString(" [label=\"")
	s.WriteString(p.getDotLabel())
	s.WriteString("\"")
	if len(p.Cond) > 0 {
		s.WriteString(" shape=diamond color=black fillcolor=aquamarine style=filled")
	} else if len(p.Graph) > 0 {
		s.WriteString(" shape=box3d, color=blue fillcolor=aquamarine style=filled")
	} else {
		s.WriteString(" color=black fillcolor=linen style=filled")
	}
	s.WriteString("];\n")
}

func (p *Vertex) dumpDotEdge(s *strings.Builder) {
	//log.Printf("Dump edge for %s/%s with deps:%d", p.g.Name, p.getDotLabel(), len(p.depsResults))
	if len(p.ExpectConfig) > 0 {
		expectConfigID := p.g.Name + "_" + p.ExpectConfig
		expectConfigID = strings.ReplaceAll(expectConfigID, "!", "")
		s.WriteString("    ")
		s.WriteString(expectConfigID)
		s.WriteString(" -> ")
		s.WriteString(p.getDotID())
		if p.ExpectConfig[0] == '!' {
			s.WriteString(" [style=dashed color=red label=\"err\"];\n")
		} else {
			s.WriteString(" [style=bold label=\"ok\"];\n")
		}

		s.WriteString("    ")
		s.WriteString(p.g.Name + "__START__")
		s.WriteString(" -> ")
		s.WriteString(expectConfigID + ";\n")
	}
	if nil == p.successorVertex || len(p.successorVertex) == 0 {
		s.WriteString("    " + p.getDotID() + " -> " + p.g.Name + "__STOP__;\n")
	}
	if nil == p.depsExpectResults || len(p.depsExpectResults) == 0 {
		s.WriteString("    " + p.g.Name + "__START__ -> " + p.getDotID() + ";\n")
	}

	if nil != p.depsExpectResults && len(p.depsExpectResults) > 0 {
		for id, idx := range p.depsVertexIdxMap {
			expect := p.depsExpectResults[idx]
			dep := p.g.getVertexByID(id)
			s.WriteString("    " + dep.getDotID() + " -> " + p.getDotID())
			switch expect {
			case ResultOk:
				s.WriteString(" [style=dashed label=\"ok\"];\n")
			case ResultErr:
				s.WriteString(" [style=dashed color=red label=\"err\"];\n")
			default:
				s.WriteString(" [style=bold label=\"all\"];\n")
			}
		}
	}
}

func (p *Vertex) findVertexInSuccessors(v *Vertex, visisted map[string]bool) bool {
	visisted[p.ID] = true
	if nil != p.successorVertex {
		_, exist := p.successorVertex[v.ID]
		if exist {
			return true
		}
		for _, successor := range p.successorVertex {
			if _, exist := visisted[successor.ID]; !exist {
				if successor.findVertexInSuccessors(v, visisted) {
					return true
				}
			}
		}
	}
	return false
}

func (p *Vertex) isSuccessorsEmpty() bool {
	return nil == p.successorVertex || len(p.successorVertex) == 0
}
func (p *Vertex) isDepsEmpty() bool {
	return nil == p.depsExpectResults || len(p.depsExpectResults) == 0
}

func (p *Vertex) verify() error {
	if !p.Start {
		if p.isDepsEmpty() && p.isSuccessorsEmpty() {
			return fmt.Errorf("vertex:%s/%s has no deps and successors", p.g.Name, p.getDotLabel())
		}
	} else {
		if !p.isDepsEmpty() {
			return fmt.Errorf("vertex:%s/%s is start vertex, but has non empty deps", p.g.Name, p.getDotLabel())
		}
	}
	return nil
}

func (p *Vertex) getDotID() string {
	return p.g.Name + "_" + p.ID
}

func (p *Vertex) getDotLabel() string {
	if len(p.Cond) > 0 {
		return strings.ReplaceAll(p.Cond, "\"", "\\\"")
	}
	if len(p.Operator) > 0 {
		if !p.isIDGenerated {
			return p.ID
		}
		// if len(p.SelectArgs) > 0{
		// 	ss :=
		// }
		return p.Operator
	}
	if len(p.Graph) > 0 {
		return fmt.Sprintf("%s::%s", p.Cluster, p.Graph)
	}
	return "unknown"
}

func (p *Vertex) buildInputOutput() error {
	if len(p.Operator) == 0 {
		return nil
	}
	op, err := GetOperator(p.Operator)
	if nil != err {
		return err
	}
	for _, opInput := range op.Input {
		match := false
		for i := range p.Input {
			if p.Input[i].Field == opInput.Name {
				match = true
				p.Input[i].opField = opInput
				p.Input[i].IsMapInput = opInput.IsMap
				p.Input[i].IsInOut = opInput.IsInput && opInput.IsOutput
				if opInput.IsExtern && !p.Input[i].IsExtern {
					p.Input[i].IsExtern = true
				}
				if !p.Input[i].Move && opInput.IsInput && opInput.IsOutput {
					p.Input[i].Move = true
				}
				break
			}
		}
		if !match {
			filed := GraphData{
				ID:         opInput.Name,
				Field:      opInput.Name,
				IsExtern:   opInput.IsExtern,
				IsInOut:    opInput.IsInput && opInput.IsOutput,
				IsMapInput: opInput.IsMap,
				opField:    opInput,
				Move:       opInput.IsInput && opInput.IsOutput,
			}
			p.Input = append(p.Input, filed)
		}
	}

	for _, opOutput := range op.Output {
		match := false
		for i := range p.Output {
			if p.Output[i].Field == opOutput.Name {
				match = true
				p.Output[i].opField = opOutput
				p.Output[i].IsInOut = opOutput.IsInput && opOutput.IsOutput
				break
			}
		}
		if !match {
			filed := GraphData{
				ID:      opOutput.Name,
				Field:   opOutput.Name,
				opField: opOutput,
				IsInOut: opOutput.IsInput && opOutput.IsOutput,
			}
			p.Output = append(p.Output, filed)
		}
	}
	return nil
}

func (p *Vertex) depend(v *Vertex, expected int) {
	idx := len(p.depsExpectResults)
	if nil == p.depsVertexIdxMap {
		p.depsVertexIdxMap = make(map[string]int)
	}
	if _, exist := p.depsVertexIdxMap[v.ID]; exist {
		return
	}

	p.depsVertexIdxMap[v.ID] = idx
	p.depsExpectResults = append(p.depsExpectResults, expected)
	if nil == v.successorVertex {
		v.successorVertex = make(map[string]*Vertex)
	}
	v.successorVertex[p.ID] = p
}

func (p *Vertex) buildDeps(deps []string, expectedResult int) error {
	for _, id := range deps {
		dep := p.g.getVertexByID(id)
		if nil == dep {
			return fmt.Errorf("[%s/%s]No dep vertex id:%s", p.g.Name, p.getDotLabel(), id)
		}
		p.depend(dep, expectedResult)
	}
	return nil
}

func (p *Vertex) buildSuccessor(sucessors []string, expectedResult int) error {
	for _, id := range sucessors {
		successor := p.g.getVertexByID(id)
		if nil == successor {
			return fmt.Errorf("[%s]No successor id:%s", p.getDotLabel(), id)
		}
		successor.depend(p, expectedResult)
	}
	return nil
}

func (p *Vertex) build() error {
	for _, cond := range p.SelectArgs {
		if !cond.IsCondExpr() && !p.g.cluster.ContainsConfigSetting(cond.Match) {
			return fmt.Errorf("no config_setting with name:%s defined", cond.Match)
		}
	}

	for _, data := range p.Input {
		if len(data.Aggregate) == 0 && !data.IsMapInput {
			id := DataID{
				Name:     data.ID,
				TypeName: data.opField.OrigInputType.Name(),
				TypeKind: data.opField.OrigInputType.Kind(),
			}
			dep := p.g.getVertexByDataID(id)
			if nil == dep && !data.IsExtern {
				return fmt.Errorf("[%s/%s]No dep input id:%s[%v]", p.g.Name, p.getDotLabel(), data.ID, id.TypeName)
			}
			if nil == dep {
				continue
			}
			if data.IsInOut && dep == p {
				continue
			}
			if data.Required {
				p.depend(dep, ResultOk)
			} else {
				p.depend(dep, ResultAll)
			}
		} else {
			for _, id := range data.Aggregate {
				dataID := DataID{
					Name:     id,
					TypeName: data.opField.OrigInputType.Name(),
					TypeKind: data.opField.OrigInputType.Kind(),
				}
				dep := p.g.getVertexByDataID(dataID)
				if nil == dep && !data.IsExtern {
					return fmt.Errorf("[%s/%s]No dep input id:%s[%v]", p.g.Name, p.getDotLabel(), data.ID, dataID.TypeName)
				}
				if nil == dep {
					continue
				}
				if data.Required {
					p.depend(dep, ResultOk)
				} else {
					p.depend(dep, ResultAll)
				}
			}
		}
	}

	if err := p.buildAllDeps(); err != nil {
		DefaultLogger.Errorf("Faild to build vertex:%v deps with err:%v", p.Operator, err)
		return err
	}
	p.SuccessorOnErr = append(p.SuccessorOnErr, p.SuccessorOnErr1...)
	p.SuccessorOnOk = append(p.SuccessorOnOk, p.SuccessorOnOk1...)
	if err := p.buildAllSuccessors(); err != nil {
		DefaultLogger.Errorf("Faild to build vertex:%v successor with err:%v", p.Operator, err)
		return err
	}
	return nil
}

func (p *Vertex) buildAllSuccessors() error {
	// DefaultLogger.Debugf("%v/%v buildAllSuccessors:%v/%v/%v", p.ID, p.Operator, p.SuccessorOnErr, p.SuccessorOnOk, p.Successor)
	if err := p.buildSuccessor(p.SuccessorOnErr, ResultErr); nil != err {
		return err
	}
	if err := p.buildSuccessor(p.SuccessorOnOk, ResultOk); nil != err {
		return err
	}
	if err := p.buildSuccessor(p.Successor, ResultAll); nil != err {
		return err
	}
	return nil
}

func (p *Vertex) buildAllDeps() error {
	if err := p.buildDeps(p.DepsOnErr, ResultErr); nil != err {
		return err
	}
	if err := p.buildDeps(p.DepsOnOk, ResultOk); nil != err {
		return err
	}
	if err := p.buildDeps(p.Deps, ResultAll); nil != err {
		return err
	}
	return nil
}

type ConfigSetting struct {
	Name string `toml:"name"`
	Cond string `toml:"cond"`
	// Processor string `toml:"processor"`
}

type Graph struct {
	Name          string    `toml:"name"`
	Vertex        []*Vertex `toml:"vertex"`
	ExpectVersion string    `toml:"expect_version"`
	Priority      int       `toml:"priority"`

	cluster     *GraphCluster
	vertexMap   map[string]*Vertex
	dataMapping map[DataID]*Vertex

	genIdx   int
	fullName string
}

func newGraph(name string, cluster *GraphCluster) *Graph {
	return &Graph{
		Name:        name,
		vertexMap:   make(map[string]*Vertex),
		dataMapping: make(map[DataID]*Vertex),
		cluster:     cluster,
	}
}

func (p *Graph) AddVertex(id string) (*Vertex, error) {
	if _, exist := p.vertexMap[id]; exist {
		return nil, ErrDuplicateName
	}
	v := &Vertex{
		ID: id,
	}
	p.Vertex = append(p.Vertex, v)
	p.vertexMap[id] = v
	return v, nil
}

func (p *Graph) dumpDot(buffer *strings.Builder) {
	buffer.WriteString("  subgraph cluster_")
	buffer.WriteString(p.Name)
	buffer.WriteString("{\n")
	buffer.WriteString("    style = rounded;\n")
	buffer.WriteString(fmt.Sprintf("    label = \"%s\";\n", p.Name))
	buffer.WriteString("    ")
	buffer.WriteString(p.Name + "__START__")
	buffer.WriteString("[color=black fillcolor=deepskyblue style=filled shape=Msquare label=\"START\"];\n")
	buffer.WriteString("    ")
	buffer.WriteString(p.Name + "__STOP__")
	buffer.WriteString("[color=black fillcolor=deepskyblue style=filled shape=Msquare label=\"STOP\"];\n")

	for _, v := range p.vertexMap {
		v.dumpDotDefine(buffer)
	}

	for _, c := range p.cluster.ConfigSetting {
		buffer.WriteString("    ")
		buffer.WriteString(p.Name + "_" + c.Name)
		buffer.WriteString(" [label=\"")
		buffer.WriteString(c.Name)
		buffer.WriteString("\"")
		buffer.WriteString(" shape=diamond color=black fillcolor=aquamarine style=filled];\n")
	}

	for _, v := range p.vertexMap {
		if v.isGenerated {
			continue
		}
		v.dumpDotEdge(buffer)
	}
	buffer.WriteString("};\n")
}

func (p *Graph) genVertexID() string {
	id := fmt.Sprintf("%s_%d", p.Name, p.genIdx)
	p.genIdx++
	return id
}
func (p *Graph) getVertexByDataID(id DataID) *Vertex {
	v, exist := p.dataMapping[id]
	if exist {
		return v
	}
	return nil
}
func (p *Graph) getVertexByID(id string) *Vertex {
	v, exist := p.vertexMap[id]
	if exist {
		return v
	}
	return nil
}

func (p *Graph) genCondVertex(cond string) *Vertex {
	v := &Vertex{}
	v.ID = p.genVertexID()
	v.isIDGenerated = true
	v.isGenerated = true
	v.Operator = p.cluster.DefaultExprProcessor
	v.Cond = cond
	v.g = p
	p.vertexMap[v.ID] = v
	return v
}

func (p *Graph) testCircle() bool {
	for _, v := range p.vertexMap {
		visited := make(map[string]bool)
		if v.findVertexInSuccessors(v, visited) {
			log.Printf("Graph:%s has a circle with vertex:%s", p.Name, v.ID)
			return true
		}
	}
	return false
}

func (p *Graph) build() error {
	if len(p.Vertex) == 0 {
		return fmt.Errorf("graph:%s vertex empty", p.Name)
	}
	p.fullName = p.cluster.logName + "::" + p.Name
	p.vertexMap = make(map[string]*Vertex)
	p.dataMapping = make(map[DataID]*Vertex)

	for i := range p.Vertex {
		v := p.Vertex[i]
		if len(v.While) > 0 {
			if len(v.Graph) == 0 {
				return fmt.Errorf("vertex:%s must config 'Graph/Cluster' as while execution.", v.While)
			}
			v.Operator = p.cluster.DefaultWhileProcessor
		}
		if len(v.Cond) > 0 {
			if len(v.Operator) == 0 {
				v.Operator = p.cluster.DefaultExprProcessor
			}
			if nil == v.Args {
				v.Args = make(map[string]interface{})
			}
			v.Args[ExprParamKey] = v.Cond
		}

		if len(v.ID) == 0 {
			if len(v.Operator) > 0 {
				v.ID = v.Operator
			} else {
				v.ID = p.genVertexID()
				v.isIDGenerated = true
			}
		}

		if len(v.Graph) > 0 {
			if len(v.Cluster) == 0 || v.Cluster == "." {
				v.Cluster = p.cluster.name
			}
		}

		if len(v.Expect) > 0 && len(v.ExpectConfig) > 0 {
			return fmt.Errorf("vertex:%s can NOT both config 'expect' & 'expect_config'", v.ID)
		}
		if len(v.ExpectConfig) > 0 {
			if !p.cluster.ContainsConfigSetting(v.ExpectConfig) {
				return fmt.Errorf("no config_setting with name:%s defined", v.ExpectConfig)
			}
		}

		if _, exist := p.vertexMap[v.ID]; exist {
			return fmt.Errorf("duplcate vertex id:%s", v.ID)
		}
		v.g = p
		p.vertexMap[v.ID] = v
		// if p.cluster.StrictDsl {
		err := v.buildInputOutput()
		if nil != err {
			return err
		}
		// }
		//inOutFields := make(map[string]bool)
		for idx := range v.Input {
			data := &v.Input[idx]
			if len(data.Field) == 0 {
				return fmt.Errorf("empty data field in intput for node:%s", v.ID)
			}
			if len(data.ID) == 0 {
				data.ID = data.Field
			}
		}
		for idx := range v.Output {
			data := &v.Output[idx]
			if len(data.Field) == 0 {
				return fmt.Errorf("empty data field in output for node:%s", v.ID)
			}
			if len(data.ID) == 0 {
				data.ID = data.Field
			}
			id := DataID{
				Name:     data.ID,
				TypeName: data.opField.Type.Name(),
				TypeKind: data.opField.Type.Kind(),
			}
			if p.cluster.StrictDsl {
				if prev, exist := p.dataMapping[id]; exist {
					return fmt.Errorf("duplicate data id:%s/%v in vertex:%s/%s, prev vertex:%s", data.ID,
						id.TypeName, v.g.Name, v.getDotLabel(), prev.getDotLabel())
				}
			}
			p.dataMapping[id] = v
		}

	}
	for _, v := range p.vertexMap {
		err := v.build()
		if nil != err {
			return err
		}
	}
	for _, v := range p.vertexMap {
		if len(v.Cond) > 0 {
			continue
		}
		err := v.verify()
		if nil != err {
			return err
		}
	}
	if p.testCircle() {
		return fmt.Errorf("circle exist")
	}
	return nil
}

type GraphClusterOptions struct {
	LogName string
	File    string
}

type GraphCluster struct {
	Desc                   string          `toml:"desc"`
	StrictDsl              bool            `toml:"strict_dsl"`
	DefaultExprProcessor   string          `toml:"default_expr_processor"`
	DefaultWhileProcessor  string          `toml:"default_while_processor"`
	DefaultDefaultPoolSize int             `toml:"default_context_pool_size"`
	Graph                  []*Graph        `toml:"graph"`
	ConfigSetting          []ConfigSetting `toml:"config_setting"`

	name    string
	logName string

	graphMap            map[string]*Graph
	contextPool         sync.Pool
	defaultExprOperator *OperatorRegistry
}

func LoadGraphCluster(file string) (*GraphCluster, error) {
	return LoadGraphClusterWithOptions(&GraphClusterOptions{
		File: file,
	})
}

func LoadGraphClusterWithOptions(options *GraphClusterOptions) (*GraphCluster, error) {
	logRegistryWarnMessages()
	cluster := NewGraphCluster("")
	if _, err := toml.DecodeFile(options.File, cluster); err != nil {
		DefaultLogger.Errorf("Failed to load graph cluster with err:%v", err)
		return nil, err
	}
	cluster.name = filepath.Base(options.File)
	cluster.logName = options.LogName
	if len(cluster.logName) == 0 {
		cluster.logName = cluster.name
	}
	err := cluster.Build()
	if nil != err {
		return nil, err
	}
	return cluster, nil
}

func NewGraphCluster(name string) *GraphCluster {
	c := &GraphCluster{
		name:     name,
		graphMap: make(map[string]*Graph),
	}
	c.contextPool = sync.Pool{
		New: func() interface{} {
			val := newGraphClusterExecuteContext(c)
			val.build()
			val.Reset()
			return val
		},
	}
	return c
}

func (p *GraphCluster) ContainsConfigSetting(name string) bool {
	for _, c := range p.ConfigSetting {
		if len(name) > 0 && name[0] == '!' {
			if c.Name == name[1:] {
				return true
			}
		} else {
			if c.Name == name {
				return true
			}
		}

	}
	return false
}

func (p *GraphCluster) AddGraph(name string) (*Graph, error) {
	_, exist := p.graphMap[name]
	if exist {
		return nil, ErrDuplicateName
	}
	g := newGraph(name, p)
	p.Graph = append(p.Graph, g)
	p.graphMap[name] = g
	return g, nil
}

func (p *GraphCluster) Build() error {
	// if len(p.ConfigSetting) > 0 && len(p.DefaultExprProcessor) == 0 {
	// 	return fmt.Errorf("need default_expr_processor = \"ispine_didagle_expr\"")
	// }
	if len(p.DefaultExprProcessor) == 0 {
		p.DefaultExprProcessor = DefaultExprOperatorName
	}
	if len(p.DefaultWhileProcessor) == 0 {
		p.DefaultWhileProcessor = DefaultWhileOperatorName
	}
	if len(p.Graph) == 0 {
		return fmt.Errorf("graph empty")
	}
	defaultExprOperator, err := GetOperator(p.DefaultExprProcessor)
	if nil != err {
		return err
	}
	p.defaultExprOperator = defaultExprOperator
	_, err = GetOperator(p.DefaultWhileProcessor)
	if nil != err {
		return err
	}

	p.graphMap = make(map[string]*Graph)
	for i := range p.Graph {
		g := p.Graph[i]
		g.cluster = p
		if existg, exist := p.graphMap[g.Name]; exist {
			if existg.ExpectVersion == g.ExpectVersion {
				return fmt.Errorf("duplicate graph name:%v", g.Name)
			}
			if g.Priority <= existg.Priority {
				continue
			}
		}
		p.graphMap[g.Name] = g
	}

	for _, g := range p.graphMap {
		err := g.build()
		if nil != err {
			return err
		}
	}
	if p.DefaultDefaultPoolSize == 0 {
		p.DefaultDefaultPoolSize = 1
	}
	for i := 0; i < p.DefaultDefaultPoolSize; i++ {
		val := newGraphClusterExecuteContext(p)
		val.Reset()
		err := val.build()
		if nil != err {
			DefaultLogger.Errorf("Failed to build context:%v", err)
			return err
		}
		p.contextPool.Put(val)
	}
	saveCluster(p)
	return nil
}

func (p *GraphCluster) dumpDot(buffer *strings.Builder) {
	buffer.WriteString("digraph G {\n")
	buffer.WriteString("    rankdir=LR;\n")
	for i := len(p.Graph) - 1; i >= 0; i-- {
		p.Graph[i].dumpDot(buffer)
	}
	buffer.WriteString("}\n")
}
