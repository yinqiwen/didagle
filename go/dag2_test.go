package didagle

import (
	"context"
	"fmt"
	"testing"
)

type demoOp0 struct {
	// dagRPCReq *helloworld.HelloRequest `didagle:"input,extern"`
	op0Result  string `didagle:"output"`
	testStrArg string `didagle:"param"`
}

func (op *demoOp0) Setup(args map[string]interface{}) error {
	return nil
}
func (op *demoOp0) Reset() {
}
func (op *demoOp0) Execute(ctx context.Context, args *Params) error {
	op.op0Result = op.testStrArg + "#testOp0"
	return nil
}

type demoOp1 struct {
	op0Result *string `didagle:"input"`
	op1Result string  `didagle:"output"`
}

func (op *demoOp1) Setup(args map[string]interface{}) error {
	return nil
}
func (op *demoOp1) Reset() {
}
func (op *demoOp1) Execute(ctx context.Context, args *Params) error {
	op.op1Result = *op.op0Result + "#testOp1"
	return nil
}

type demoOp2 struct {
	op0Result *string `didagle:"input"`
	op2Result string  `didagle:"output"`
}

func (op *demoOp2) Setup(args map[string]interface{}) error {
	return nil
}
func (op *demoOp2) Reset() {
}
func (op *demoOp2) Execute(ctx context.Context, args *Params) error {
	op.op2Result = *op.op0Result + "#testOp2"
	return nil
}

type testOut struct {
	V int
}

type demoOp3 struct {
	op1Result *string `didagle:"input"`
	op2Result *string `didagle:"input"`
	op3Result string  `didagle:"output"`
	testOut   testOut `didagle:"output,auto_clear"`
}

func (op *demoOp3) Setup(args map[string]interface{}) error {
	return nil
}
func (op *demoOp3) Reset() {
	fmt.Printf("###After reset, testOut.V=%v\n", op.testOut.V)
}
func (op *demoOp3) Execute(ctx context.Context, args *Params) error {
	if nil != op.op1Result {
		op.op3Result = *op.op1Result + "#testOp3"
	}
	if nil != op.op2Result {
		op.op3Result = *op.op2Result + "#testOp3"
	}
	op.testOut.V = 1000
	return nil
}

type demoOp4 struct {
	op3Result *string                `didagle:"input"`
	jsonOut   map[string]interface{} `didagle:"input,extern"`
	loopIdx   int                    `didagle:"param"`
}

func (op *demoOp4) Setup(args map[string]interface{}) error {
	return nil
}
func (op *demoOp4) Reset() {
}
func (op *demoOp4) Execute(ctx context.Context, args *Params) error {
	op.jsonOut[fmt.Sprintf("%d", op.loopIdx)] = *op.op3Result + "#demoOp4"
	return nil
}

func initDags2() {
	RegisterOperator("op0", new(demoOp0))
	RegisterOperator("op1", new(demoOp1))
	RegisterOperator("op2", new(demoOp2))
	RegisterOperator("op3", new(demoOp3))
	RegisterOperator("op4", new(demoOp4))
	_, err := LoadGraphCluster("./testdata/test.toml")
	if nil != err {
		fmt.Printf("##########Load error:%v\n", err)
	}
}

func TestWhile(t *testing.T) {
	//testOnce1.Do(initDags2)
	initDags2()
	// expect false
	ctx := NewExecuteContext(nil)
	params := NewParams(map[string]interface{}{"flag": 1})
	jsonDict := make(map[string]interface{})
	ctx.Set("jsonOut", jsonDict)

	err := ExecuteGraph(nil, "test.toml", "main", ctx, params)
	if nil != err {
		t.Errorf("%v", err)
	}
	t.Logf("%v", jsonDict)

}
