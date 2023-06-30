package didagle

import (
	"context"
	"fmt"
	"sync"
	"testing"
)

type testOp0 struct {
	testIntf   F                 `didagle:"input,extern"`
	testMap    map[string]string `didagle:"input,extern"`
	testSlice  []string          `didagle:"input,extern"`
	testMove   *string           `didagle:"input,output,extern"`
	mydata     string            `didagle:"output"`
	outputIntf F                 `didagle:"output"`
	param0     int               `didagle:"param"`
	param1     string            `didagle:"param"`
}

func (op *testOp0) Setup(args map[string]interface{}) error {
	return nil
}
func (op *testOp0) Execute(ctx context.Context, args *Params) error {
	if nil != op.testIntf {
		op.testIntf.Log("From testOp0")
	} else {
		fmt.Printf("##testOp0 empty testIntf\n")
	}
	fmt.Printf("##testOp0 testMap:%v testSlice:%v param0:%v, param1:%v\n", op.testMap, op.testSlice, op.param0, op.param1)
	op.mydata = "hhadadas"
	if nil != op.testMove {
		*op.testMove = *op.testMove + ":this is move data"
		fmt.Printf("##op0 has set testMove:%v\n", *op.testMove)
	} else {
		fmt.Printf("##op0 has empty testMove\n")
	}
	op.outputIntf = &FF{}
	return nil
}
func (op *testOp0) Reset() {
	op.mydata = ""
}

type testOp1 struct {
	mydata     *string `didagle:"input"`
	outputIntf F       `didagle:"input"`
	mydata2    *string `didagle:"input,extern"`
	testMove   *string `didagle:"input,extern"`
	// mymap      map[string]*string `didagle:"input,map"`
}

func (op *testOp1) Setup(args map[string]interface{}) error {
	return nil
}
func (op *testOp1) Execute(ctx context.Context, args *Params) error {
	if nil != op.outputIntf {
		op.outputIntf.Log("From testOp1")
	} else {
		fmt.Printf("##testOp1 empty outputIntf\n")
	}
	if nil != op.mydata {
		fmt.Printf("##op1 has  mydata:%s\n", *op.mydata)
	} else {
		fmt.Printf("##op1 has empty mydata\n")
	}
	if nil != op.mydata2 {
		fmt.Printf("##op1 has  mydata2:%s\n", *op.mydata2)
	} else {
		fmt.Printf("##op1 has empty mydata2\n")
	}
	if nil != op.testMove {
		fmt.Printf("##op1 has testMove:%s\n", *op.testMove)
	} else {
		fmt.Printf("##op1 has empty testMove\n")
	}
	return nil
}
func (op *testOp1) Reset() {
	op.mydata = nil
	// op.mymap = make(map[string]*string)
}

type F interface {
	Log(string)
}

type FF struct {
	val string
}

func (op *FF) Log(msg string) {
	fmt.Printf("%v\n", msg)
}

type testOp000 struct {
	*DefaultOperator
	str     string  `didagle:"output"`
	integer int     `didagle:"output"`
	float   float32 `didagle:"output"`
	obj     FF      `didagle:"output"`
	intf    F       `didagle:"output"`
}

func (op *testOp000) Execute(ctx context.Context, args *Params) error {
	fmt.Printf("testOp000 exec!\n")
	op.str = "from testOp000"
	op.integer = 1213
	op.float = 3.14
	op.obj.val = "hello,world"
	op.intf = &FF{val: "interface val"}
	return nil
}

type testOp001 struct {
	*DefaultOperator
	str     *string  `didagle:"input"`
	integer *int     `didagle:"input"`
	float   *float32 `didagle:"input"`
	obj     *FF      `didagle:"input"`
	intf    F        `didagle:"input"`
}

func (op *testOp001) Execute(ctx context.Context, args *Params) error {
	fmt.Printf("testOp001 exec with str:%v, int:%v, float:%v, obj:%v, intf:%v\n", *op.str, *op.integer, *op.float, op.obj, op.intf)
	return nil
}

type testOp010 struct {
	*DefaultOperator
	errFlag bool `didagle:"param"`
}

func (op *testOp010) Execute(ctx context.Context, args *Params) error {
	fmt.Printf("testOp010 exec with errFlag:%v!\n", op.errFlag)
	if op.errFlag {
		return fmt.Errorf("err")
	} else {
		return nil
	}
}

type testOp011 struct {
	*DefaultOperator
}

func (op *testOp011) Execute(ctx context.Context, args *Params) error {
	fmt.Printf("testOp011 exec!\n")
	return nil
}

type testOp012 struct {
	*DefaultOperator
}

func (op *testOp012) Execute(ctx context.Context, args *Params) error {
	fmt.Printf("testOp012 exec!\n")
	return nil
}

var testOnce0 sync.Once

func initTestDags0() {
	RegisterOperator("phase000", new(testOp000))
	RegisterOperator("phase001", new(testOp001))
	RegisterOperator("phase010", new(testOp010))
	RegisterOperator("phase011", new(testOp011))
	RegisterOperator("phase012", new(testOp012))

	_, err := LoadGraphCluster("./testdata/dag0.toml")
	if nil != err {
		fmt.Printf("##########Load error:%v\n", err)
	}
}

// func TestSimple(t *testing.T) {
// 	err := RegisterOperator("op0", new(testOp0))
// 	if nil != err {
// 		t.Errorf("%v", err)
// 	}
// 	err = RegisterOperator("op1", new(testOp1))
// 	if nil != err {
// 		t.Errorf("%v", err)
// 	}

// 	cluster := NewGraphCluster("test")
// 	cluster.StrictDsl = true
// 	g, err := cluster.AddGraph("demo")
// 	if nil != err {
// 		t.Errorf("%v", err)
// 	}
// 	v0, err := g.AddVertex("v0")
// 	if nil != err {
// 		t.Errorf("%v", err)
// 	}
// 	v1, err := g.AddVertex("v1")
// 	if nil != err {
// 		t.Errorf("%v", err)
// 	}
// 	v0.Operator = "op0"
// 	v1.Operator = "op1"
// 	err = cluster.Build()
// 	if nil != err {
// 		t.Errorf("%v", err)
// 	}

// 	ctx := NewExecuteContext(nil)
// 	testVal := "hello,world"
// 	SetExecuteContext[*string](ctx, "mydata2", &testVal)
// 	SetExecuteContext[F](ctx, "testIntf", &FF{})
// 	testMap := make(map[string]string)
// 	testMap["hello"] = "world"
// 	SetExecuteContext[map[string]string](ctx, "testMap", testMap)
// 	testSlice := []string{"hello,world"}
// 	SetExecuteContext[[]string](ctx, "testSlice", testSlice)
// 	testMoveStr := "hello,world"
// 	SetExecuteContext[*string](ctx, "testMove", &testMoveStr)
// 	// ctx.Set("mydata2", &testVal) // inject external data
// 	params := NewParams(map[string]interface{}{"param0": int(101), "param1": "aaa"})

// 	err = ExecuteGraph(nil, "test", "demo", ctx, params)
// 	if nil != err {
// 		t.Errorf("%v", err)
// 	}

// 	err = ExecuteGraph(nil, "test", "demo", ctx, params)
// 	if nil != err {
// 		t.Errorf("%v", err)
// 	}
// }

func TestDAG0_0(t *testing.T) {
	testOnce0.Do(initTestDags0)
	// Auto Graph
	ctx := NewExecuteContext(nil)
	params := NewParams(map[string]interface{}{})

	err := ExecuteGraph(nil, "dag0.toml", "graph0", ctx, params)
	if nil != err {
		t.Errorf("%v", err)
	}
}

func TestDAG0_1(t *testing.T) {
	testOnce0.Do(initTestDags0)
	// Successor

	ctx := NewExecuteContext(nil)
	params := NewParams(map[string]interface{}{})
	err := ExecuteGraph(nil, "dag0.toml", "graph1", ctx, params)
	if nil != err {
		t.Errorf("%v", err)
	}
}

func TestDAG0_2(t *testing.T) {
	testOnce0.Do(initTestDags0)
	// Dep
	ctx := NewExecuteContext(nil)
	params := NewParams(map[string]interface{}{"errFlag": false})

	err := ExecuteGraph(nil, "dag0.toml", "graph2", ctx, params)
	if nil != err {
		t.Errorf("%v", err)
	}
}

func TestDAG0_3(t *testing.T) {
	testOnce0.Do(initTestDags0)
	// if/else
	ctx := NewExecuteContext(nil)
	params := NewParams(map[string]interface{}{"errFlag": true})

	err := ExecuteGraph(nil, "dag0.toml", "graph3", ctx, params)
	if nil != err {
		t.Errorf("%v", err)
	}

	params = NewParams(map[string]interface{}{"errFlag": false})
	err = ExecuteGraph(nil, "dag0.toml", "graph3", ctx, params)
	if nil != err {
		t.Errorf("%v", err)
	}
}

// type T struct {
// 	v F
// }

// var tmap = make(map[string]dataValue)

// func setInterface[T any](name string, val T) error {
// 	t := reflect.TypeOf((*T)(nil)).Elem()
// 	if t.Kind() != reflect.Interface {
// 		pp := (*iface)(unsafe.Pointer(&val))
// 		tmap[name] = dataValue{Type: uintptr(pp.Type), Data: uintptr(pp.Data), Val: val}
// 	} else {
// 		pp := (*iface)(unsafe.Pointer(&val))
// 		fmt.Printf("##Set interface %v %v\n", pp.Data, pp.Type)
// 		tmap[name] = dataValue{Type: uintptr(pp.Type), Data: uintptr(pp.Data), Val: val}
// 	}
// 	return nil
// }
// func getInterface[T any](name string) (T, error) {
// 	t := reflect.TypeOf((*T)(nil)).Elem()
// 	val, exist := tmap[name]
// 	if !exist {
// 		var empty T
// 		return empty, errFalse
// 	}
// 	if t.Kind() != reflect.Interface {
// 		return val.Val.(T), nil
// 	} else {
// 		var r T
// 		vv := (*iface)(unsafe.Pointer(&r))
// 		vv.Data = unsafe.Pointer(val.Data)
// 		vv.Type = unsafe.Pointer(val.Type)
// 		fmt.Printf("##Get interface %v %v\n", vv.Data, vv.Type)
// 		return r, nil
// 	}
// }

// func TestReflect(t *testing.T) {
// 	test := &T{}
// 	f := &FF{}
// 	// test.v = f

// 	var val F
// 	val = f
// 	v := (*iface)(unsafe.Pointer(&val))
// 	p := unsafe.Pointer(test)
// 	fmt.Printf("1####%v\n", unsafe.Pointer(&test.v))

// 	fmt.Printf("1####%v\n", unsafe.Pointer(&test.v))
// 	offset := reflect.TypeOf(*test).Field(0).Offset
// 	fmt.Printf("is interface:%v\n", reflect.TypeOf(*test).Field(0).Type.Kind() == reflect.Interface)
// 	fieldPtr := uintptr(p) + offset
// 	vv2 := (*iface)(unsafe.Pointer(fieldPtr))
// 	fmt.Printf("2####%v %v\n", unsafe.Pointer(vv2.Data), unsafe.Pointer(vv2.Type))
// 	// test.v = f
// 	fmt.Printf("2####%v %v\n", unsafe.Pointer(vv2.Data), unsafe.Pointer(vv2.Type))
// 	fmt.Printf("3####%v %v\n", unsafe.Pointer(v.Data), unsafe.Pointer(v.Type))
// 	vv2.Data = v.Data
// 	vv2.Type = v.Type
// 	test.v.Log("adadada")
// 	// vv2.Type = v.Type
// 	// fmt.Printf("%v %v %v %v %v\n", unsafe.Pointer(f), v.Data, vv2.Data, unsafe.Pointer(fieldPtr), unsafe.Pointer(&test.v))
// 	// *(*iface)(unsafe.Pointer(fieldPtr)) = *(*iface)(unsafe.Pointer(&val))
// 	// ttt := reflect.TypeOf((*Operator)(nil)).Elem()
// 	// ttt := reflect.TypeOf((*T)(nil)).Elem()
// 	// fmt.Printf("###%v %v\n", ttt.Name(), ttt.Kind() == reflect.Interface)

// 	setInterface[string]("a", "ass")
// 	ss, err := getInterface[string]("a")
// 	fmt.Printf("##%v %v\n", ss, err)

// 	setInterface[int64]("b", 1201)
// 	ii, err := getInterface[int64]("b")
// 	fmt.Printf("##%v %v\n", ii, err)

// 	setInterface[F]("f", &FF{})
// 	ff, err := getInterface[F]("f")
// 	fmt.Printf("##%v %v\n", ff, err)
// 	ff.Log("aaa")
// }

// type customSelector struct{}

// func (s *customSelector) SelectGVal(ctx context.Context, k string) (interface{}, error) {
// 	fmt.Printf("Select key:%v\n", k)
// 	return "custom_val", nil
// }

// func TestEval(t *testing.T) {
// 	eval, err := gval.Full().NewEvaluable("myStruct.hidden != \"custom_val\"")
// 	if err != nil {
// 		fmt.Println(err)
// 		return
// 	}

// 	value, err := eval.EvalString(context.Background(), map[string]interface{}{"myStruct": &customSelector{}})
// 	if err != nil {
// 		fmt.Println(err)
// 	}

// 	fmt.Printf("Val:%v\n", value)
// }
