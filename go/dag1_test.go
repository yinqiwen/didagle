package didagle

import (
	"context"
	"fmt"
	"os"
	"sync"
	"testing"
)

type phase0 struct {
	*DefaultOperator

	phase0MapOut   map[string]string `didagle:"output"`
	phase0SliceOut []string          `didagle:"output"`
}

func (op *phase0) Execute(ctx context.Context, args *Params) error {
	fmt.Printf("phase0 exec!\n")
	op.phase0MapOut = make(map[string]string)
	op.phase0MapOut["phase0"] = "value0"
	op.phase0SliceOut = append(op.phase0SliceOut, "aaaaa")
	return nil
}

type phase1 struct {
	*DefaultOperator
	key0      string    `didagle:"param"`
	counter   int       `didagle:"output"`
	loopIdx   int       `didagle:"param"`
	intArgs   []int     `didagle:"param"`
	floatArgs []float32 `didagle:"param"`
	strArgs   []string  `didagle:"param"`
	boolArgs  []bool    `didagle:"param"`

	mapInput       map[string]string `didagle:"input,extern"`
	sliceInput     []string          `didagle:"input,extern"`
	phase0MapOut   map[string]string `didagle:"input,extern"`
	phase0SliceOut []string          `didagle:"input,extern"`
}

func (op *phase1) Execute(ctx context.Context, args *Params) error {
	if nil != op.mapInput {
		op.mapInput[fmt.Sprintf("aa:%d", op.loopIdx)] = "bbb"
	}
	op.sliceInput = append(op.sliceInput, fmt.Sprintf("%d", op.loopIdx))
	fmt.Printf("[%p]phase1 exec with mapInput:%v,sliceInput:%v, key0:%v, counter:%v loopIdx:%v intArgs:%v floatArgs:%v strArgs:%v boolArgs:%v phase0MapOut%v phase0SliceOut:%v!\n", op, op.mapInput, op.sliceInput, op.key0, op.counter, op.loopIdx, op.intArgs, op.floatArgs, op.strArgs, op.boolArgs, op.phase0MapOut, op.phase0SliceOut)
	op.counter++

	return nil
}

type phase2 struct {
	*DefaultOperator
	key0 string `didagle:"param"`
	data string `didagle:"output"`
}

func (op *phase2) Execute(ctx context.Context, args *Params) error {
	fmt.Printf("phase2 exec with key0:%v!\n", op.key0)
	op.data = op.key0
	return nil
}

type phase3 struct {
	*DefaultOperator
	mapData map[string]*string `didagle:"input,map"`
}

func (op *phase3) Reset() {
	op.mapData = make(map[string]*string)
}
func (op *phase3) Execute(ctx context.Context, args *Params) error {
	for k, v := range op.mapData {
		fmt.Printf("phase3 %v->%v!\n", k, *v)
	}
	return nil
}

var testOnce1 sync.Once

func initDags() {
	RegisterOperator("phase0", new(phase0))
	RegisterOperator("phase1", new(phase1))
	RegisterOperator("phase2", new(phase2))
	RegisterOperator("phase3", new(phase3))
	_, err := LoadGraphCluster("./testdata/dag1.toml")
	if nil != err {
		fmt.Printf("##########Load error:%v\n", err)
	}
}

func TestDAG1_0(t *testing.T) {
	testOnce1.Do(initDags)
	// expect false
	ctx := NewExecuteContext(nil)
	params := NewParams(map[string]interface{}{})
	t.Logf("expect false")
	err := ExecuteGraph(nil, "dag1.toml", "graph0", ctx, params)
	if nil != err {
		t.Errorf("%v", err)
	}

	t.Logf("expect true")
	params = NewParams(map[string]interface{}{"key": "hello"})
	err = ExecuteGraph(nil, "dag1.toml", "graph0", ctx, params)
	if nil != err {
		t.Errorf("%v", err)
	}
}

func TestDAG1_1(t *testing.T) {
	testOnce1.Do(initDags)
	// expect false
	ctx := NewExecuteContext(nil)
	params := NewParams(map[string]interface{}{"flag0": 100})
	t.Logf("expect false")
	err := ExecuteGraph(nil, "dag1.toml", "graph1", ctx, params)
	if nil != err {
		t.Errorf("%v", err)
	}

	t.Logf("expect true")
	params = NewParams(map[string]interface{}{"flag0": 101, "flag1": 102})
	err = ExecuteGraph(nil, "dag1.toml", "graph1", ctx, params)
	if nil != err {
		t.Errorf("%v", err)
	}
}

func TestDAG1_2(t *testing.T) {
	testOnce1.Do(initDags)
	// expect false
	ctx := NewExecuteContext(nil)
	params := NewParams(map[string]interface{}{"flag0": 101})
	err := ExecuteGraph(nil, "dag1.toml", "graph2", ctx, params)
	if nil != err {
		t.Errorf("%v", err)
	}

	// fmt.Print(DumpAllOperatorMetaInfo())
}

func TestDAG1_3(t *testing.T) {
	testOnce1.Do(initDags)

	metas := DumpAllOperatorMetaInfo()
	os.WriteFile("dag_ops.json", []byte(metas), 0755)
	// map aggregate
	ctx := NewExecuteContext(nil)
	params := NewParams(map[string]interface{}{"flag0": 101})
	err := ExecuteGraph(nil, "dag1.toml", "graph3", ctx, params)
	if nil != err {
		t.Errorf("%v", err)
	}
	// fmt.Print(DumpAllOperatorMetaInfo())
}

func TestDAG1_4(t *testing.T) {
	testOnce1.Do(initDags)

	// map aggregate
	ctx := NewExecuteContext(nil)
	m := make(map[string]string)
	m["test"] = "world"

	ctx.Set("mapInput", m)

	sliceInput := make([]string, 0)
	sliceInput = append(sliceInput, "zzzzzz")
	ctx.Set("sliceInput", sliceInput)
	params := NewParams(map[string]interface{}{"flag0": 6, "loopCounter": 6})

	err := ExecuteGraph(nil, "dag1.toml", "while_graph", ctx, params)
	if nil != err {
		t.Errorf("%v", err)
	}
	// fmt.Print(DumpAllOperatorMetaInfo())
}
