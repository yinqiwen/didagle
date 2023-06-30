package didagle

import (
	"context"
	"errors"
	"reflect"
	"time"

	"git.code.oa.com/trpc-go/trpc-go/metrics"
)

var ErrExistParentContext = errors.New("exist parent context")

type FieldFlags struct {
	Extern    int `json:"is_extern"`
	InOut     int `json:"is_in_out"`
	Agrregate int `json:"is_aggregate"`
}

type FieldInfo struct {
	Name  string     `json:"name"`
	ID    int64      `json:"id"`
	Type  string     `json:"type"`
	Flags FieldFlags `json:"flags"`
}

type ParamInfo struct {
	Name string `json:"name"`
	Type string `json:"type"`
}

type OperatorInfo struct {
	Name   string      `json:"name"`
	Source string      `json:"source"`
	Line   int         `json:"line"`
	Input  []FieldInfo `json:"input,omitempty"`
	Output []FieldInfo `json:"output,omitempty"`
	Params []ParamInfo `json:"params,omitempty"`
}

type DataID struct {
	Name     string
	TypeName string
	TypeKind reflect.Kind
}

type Data struct {
	Ptr uintptr
}

type FieldMapping struct {
	Field string
	Name  string
}

type OperatorInstance struct {
	Name string
	op   Operator
	ptr  uintptr

	Opt Options
}

// Options 帮助算子实例记录一下当前算子的信息
type Options struct {
	CurrentGraph string
}

type Option func(*Options)

func WithCurrentGraph(graph string) Option {
	return func(o *Options) {
		o.CurrentGraph = graph
	}
}

const EnableTimerReportKey = "enableTimerReport"

// Execute 算子实例执行，可以执行一些逻辑
// todo 增加一些filter, 添加一下监控统计功能
func (op *OperatorInstance) Execute(ctx context.Context, params *Params, opts ...Option) error {
	// 参数注入了, 就进行上报
	if _, exist := params.Get(EnableTimerReportKey); exist {
		for _, o := range opts {
			o(&op.Opt)
		}
		start := time.Now()
		defer func() {
			timeCost := time.Since(start).Microseconds()
			metrics.ReportMultiDimensionMetricsX(
				"dag_operator",
				[]*metrics.Dimension{
					{Name: "graphName", Value: op.Opt.CurrentGraph},
					{Name: "operatorName", Value: op.Name},
				},
				[]*metrics.Metrics{
					metrics.NewMetrics("latency", float64(timeCost), metrics.PolicyHistogram),
				},
			)
		}()

	}

	return op.op.Execute(ctx, params)
}

func (op *OperatorInstance) Reset() {
	op.Opt.CurrentGraph = ""
}

type Operator interface {
	Setup(args map[string]interface{}) error
	Execute(context.Context, *Params) error
	Reset()
}

type DefaultOperator struct {
}

func (op *DefaultOperator) Setup(args map[string]interface{}) error {
	return nil
}
func (op *DefaultOperator) Execute(*Params) error {
	return nil
}
func (op *DefaultOperator) Reset() {
}

type noop struct {
}

func (op *noop) Setup(args map[string]interface{}) error {
	return nil
}
func (op *noop) Execute(context.Context, *Params) error {
	return nil
}
func (op *noop) Reset() {
}

func init() {
	RegisterOperator("noop", new(noop))
}
