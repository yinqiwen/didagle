package didagle

import (
	"context"
	"errors"
	"reflect"
	"sync"

	"github.com/PaesslerAG/gval"
)

const ExprParamKey = "__expr"
const WhileExecCluterParamKey = "__while_cluster"
const WhileExecGraphParamKey = "__while_graph"
const WhileAsyncExecParamKey = "__while_async_exec"
const DefaultExprOperatorName = "ispine_didagle_expr"
const DefaultWhileOperatorName = "ispine_didagle_while"

var errFalse = errors.New("false error")

type GvalExpr struct {
	eval    gval.Evaluable
	context *ExecuteContext `didagle:"context"`

	// xxx *ExecuteContext `didagle:"context"`
}

func (op *GvalExpr) Setup(args map[string]interface{}) error {
	val, exist := args[ExprParamKey]
	if !exist {
		DefaultLogger.Errorf("No '%v' found in params for GvalExpr", ExprParamKey)
		return ErrDataNotFound
	}
	expr, ok := val.(string)
	if !ok {
		return ErrInvalidArgType
	}
	eval, err := gval.Full().NewEvaluable(expr)
	if nil != err {
		return err
	}
	op.eval = eval
	return nil
}

func (op *GvalExpr) Execute(ctx context.Context, args *Params) error {
	value, err := op.eval.EvalBool(context.Background(),
		map[string]interface{}{
			"ARGS": &paramVarSelector{args: args},
			"VARS": &operatorVarSelector{execCtx: op.context}})
	if nil != err {
		return err
	}
	if value {
		return nil
	}
	return errFalse
}
func (op *GvalExpr) Reset() {
}

type GvalWhileExpr struct {
	context *ExecuteContext `didagle:"context"`

	eval    gval.Evaluable
	cluster string
	graph   string
	async   bool

	loopIdx int
}

func (op *GvalWhileExpr) Setup(args map[string]interface{}) error {
	val, exist := args[ExprParamKey]
	if !exist {
		return ErrDataNotFound
	}
	expr, ok := val.(string)
	if !ok {
		return ErrInvalidArgType
	}
	eval, err := gval.Full().NewEvaluable(expr)
	if nil != err {
		return err
	}
	op.eval = eval

	val, exist = args[WhileExecCluterParamKey]
	if !exist {
		return ErrDataNotFound
	}
	op.cluster, ok = val.(string)
	if !ok {
		return ErrInvalidArgType
	}

	val, exist = args[WhileExecGraphParamKey]
	if !exist {
		return ErrDataNotFound
	}
	op.graph, ok = val.(string)
	if !ok {
		return ErrInvalidArgType
	}

	val, exist = args[WhileAsyncExecParamKey]
	if !exist {
		return ErrDataNotFound
	}
	op.async, ok = val.(bool)
	if !ok {
		return ErrInvalidArgType
	}
	return nil
}

func (op *GvalWhileExpr) Execute(ctx context.Context, args *Params) error {
	err := errFalse
	testValue := false
	wg := sync.WaitGroup{}
	for {
		subParams := NewParams(nil)
		subParams.setParent(args)
		subParams.set("loopIdx", op.loopIdx)
		testValue, err = op.eval.EvalBool(context.Background(),
			map[string]interface{}{
				"ARGS": &paramVarSelector{args: subParams},
				"VARS": &operatorVarSelector{execCtx: op.context}})
		if nil != err {
			break
		}
		if testValue {
			wg.Add(1)
			if op.async {
				go func() {
					ExecuteGraph(ctx, op.cluster, op.graph, op.context, subParams)
					wg.Done()
				}()
			} else {
				err = ExecuteGraph(ctx, op.cluster, op.graph, op.context, subParams)
				if nil != err {
					break
				}
			}
			op.loopIdx++
			continue
		} else {
			err = errFalse
			break
		}
	}
	if op.async {
		wg.Wait()
	}
	return err
}
func (op *GvalWhileExpr) Reset() {
}

type paramVarSelector struct {
	args *Params
}

func (s *paramVarSelector) SelectGVal(ctx context.Context, key string) (interface{}, error) {
	if nil != s.args {
		val, exist := s.args.Get(key)
		if exist {
			return val, nil
		}
	}
	// use false as return value
	return false, nil
}

type operatorVarSelector struct {
	execCtx *ExecuteContext
}

func (s *operatorVarSelector) SelectGVal(ctx context.Context, key string) (interface{}, error) {
	if nil != s.execCtx {
		id := DataID{
			Name:     key,
			TypeName: "bool",
			TypeKind: reflect.Bool,
		}
		ptr, err := s.execCtx.getVal(id)

		if nil == err {
			if nil == ptr.Data {
				return false, nil
			}
			bv := *(*bool)(ptr.Data)
			return bv, nil
		}
		id.TypeName = "string"
		id.TypeKind = reflect.String
		ptr, err = s.execCtx.getVal(id)

		if nil == err {
			if nil == ptr.Data {
				return "", nil
			}
			sv := *(*string)(ptr.Data)
			return sv, nil
		}
		id.TypeName = "int"
		id.TypeKind = reflect.Int
		ptr, err = s.execCtx.getVal(id)

		if nil == err {
			if nil == ptr.Data {
				return int(0), nil
			}
			sv := *(*int)(ptr.Data)
			return sv, nil
		}
		id.TypeName = "int64"
		id.TypeKind = reflect.Int64
		ptr, err = s.execCtx.getVal(id)
		if nil == err {
			if nil == ptr.Data {
				return int64(0), nil
			}
			sv := *(*int64)(ptr.Data)
			return sv, nil
		}
		id.TypeName = "float32"
		id.TypeKind = reflect.Float32
		ptr, err = s.execCtx.getVal(id)
		if nil == err {
			if nil == ptr.Data {
				return 0.0, nil
			}
			sv := *(*float32)(ptr.Data)
			return sv, nil
		}
		id.TypeName = "float64"
		id.TypeKind = reflect.Float64
		ptr, err = s.execCtx.getVal(id)
		if nil == err {
			if nil == ptr.Data {
				return 0.0, nil
			}
			sv := *(*float64)(ptr.Data)
			return sv, nil
		}
	}
	// use false as return value
	return false, nil
}
func init() {
	_ = RegisterOperator(DefaultExprOperatorName, new(GvalExpr))
	_ = RegisterOperator(DefaultWhileOperatorName, new(GvalWhileExpr))
}
