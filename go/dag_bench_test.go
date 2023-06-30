package didagle

import (
	"context"
	"fmt"
	"sync"
	"testing"
)

type benchPhase0 struct {
	*DefaultOperator
	arg0 string `didagle:"param"`
	v0   string `didagle:"output"`
}

func (op *benchPhase0) Execute(ctx context.Context, args *Params) error {
	op.v0 = "[from benchPhase0]"
	return nil
}

type benchPhase1 struct {
	*DefaultOperator
	arg1 int64   `didagle:"param"`
	v0   *string `didagle:"input"`
	v1   int64   `didagle:"output"`
}

func (op *benchPhase1) Execute(ctx context.Context, args *Params) error {
	op.v1 = 124
	return nil
}

type benchPhase2 struct {
	*DefaultOperator
	v0 *string `didagle:"input"`
	v2 string  `didagle:"output"`
}

func (op *benchPhase2) Execute(ctx context.Context, args *Params) error {
	op.v2 = *op.v0 + "[from benchPhase2]"
	return nil
}

type benchPhase3 struct {
	*DefaultOperator
	v1 *int64  `didagle:"input"`
	v2 *string `didagle:"input"`
	v3 string  `didagle:"output"`
}

func (op *benchPhase3) Execute(ctx context.Context, args *Params) error {
	op.v3 = *op.v2 + "[from benchPhase3]" + fmt.Sprintf("[%d]", *op.v1)
	return nil
}

type benchPhase4 struct {
	*DefaultOperator
	v1 *int64  `didagle:"input"`
	v2 *string `didagle:"input"`
	v4 string  `didagle:"output"`
}

func (op *benchPhase4) Execute(ctx context.Context, args *Params) error {
	op.v4 = "[from benchPhase4]"
	return nil
}

type benchPhase5 struct {
	*DefaultOperator
	v3 *string `didagle:"input"`
	v4 *string `didagle:"input"`
	v5 string  `didagle:"output"`
}

func (op *benchPhase5) Execute(ctx context.Context, args *Params) error {
	op.v5 = *op.v3 + *op.v4
	return nil
}

var benchOnce sync.Once

func initBenchDags() {
	RegisterOperator("bench0", new(benchPhase0))
	RegisterOperator("bench1", new(benchPhase1))
	RegisterOperator("bench2", new(benchPhase2))
	RegisterOperator("bench3", new(benchPhase3))
	RegisterOperator("bench4", new(benchPhase4))
	RegisterOperator("bench5", new(benchPhase5))

	_, err := LoadGraphCluster("./testdata/bench.toml")
	if nil != err {
		fmt.Printf("##########Load error:%v\n", err)
	}
}

// bench0 -> [bench1, bench2] ->[bench3, bench4] -> bench5

func BenchmarkDirect0(b *testing.B) {
	phase0 := &benchPhase0{}
	phase1 := &benchPhase1{}
	phase2 := &benchPhase2{}
	phase3 := &benchPhase3{}
	phase4 := &benchPhase4{}
	phase5 := &benchPhase5{}
	params := NewParams(nil)
	wg := &sync.WaitGroup{}
	f := func() {
		phase0.Execute(nil, params)
		phase1.v0 = &phase0.v0
		phase2.v0 = &phase0.v0
		wg.Add(2)
		go func() {
			phase1.Execute(nil, params)
			wg.Done()
		}()
		go func() {
			phase2.Execute(nil, params)
			wg.Done()
		}()
		wg.Wait()
		phase3.v1 = &phase1.v1
		phase4.v1 = &phase1.v1
		phase3.v2 = &phase2.v2
		phase4.v2 = &phase2.v2
		wg.Add(2)
		go func() {
			phase3.Execute(nil, params)
			wg.Done()
		}()
		go func() {
			phase4.Execute(nil, params)
			wg.Done()
		}()
		wg.Wait()
		phase5.v3 = &phase3.v3
		phase5.v4 = &phase4.v4
		phase5.Execute(nil, params)

		phase0.Reset()
		phase1.Reset()
		phase2.Reset()
		phase3.Reset()
		phase4.Reset()
		phase5.Reset()
	}

	for i := 0; i < b.N; i++ {
		f()
	}
}

func BenchmarkDAG0(b *testing.B) {
	benchOnce.Do(initBenchDags)
	for i := 0; i < b.N; i++ {
		ctx := NewExecuteContext(nil)
		params := NewParams(map[string]interface{}{})
		ExecuteGraph(nil, "bench.toml", "bench", ctx, params)
	}
}
