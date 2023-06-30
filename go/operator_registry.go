package didagle

import (
	"encoding/json"
	"errors"
	"fmt"
	"reflect"
	"runtime"
	"strings"
	"sync"
	"unsafe"
)

var ErrOpNotFound = errors.New("operator not found")
var ErrGraphNotFound = errors.New("graph not found")
var ErrVertexNotFound = errors.New("vertex not found")
var ErrDataNotFound = errors.New("data not found")
var ErrDuplicateName = errors.New("operator duplicate name")
var ErrMissingStructTag = errors.New("missing struct tag")
var ErrInvalidStructTag = errors.New("invalid struct tag")
var ErrInvalidInputFieldType = errors.New("invalid input field type")
var ErrInvalidContextFieldType = errors.New("invalid context field type")
var ErrInvalidArgType = errors.New("invalid argument type")

var registryWarnMessages []string
var warnLogOnce sync.Once

func logRegistryWarnMessages() {
	warnLogOnce.Do(func() {
		if len(registryWarnMessages) > 0 {
			DefaultLogger.Errorf("====================Operator Registry Warn Begin======================")
			eg := "Mark complex output field auto_clear example:\n" +
				"type myOp struct {\n" +
				"    testOut TestStruct `didagle:\"output,auto_clear\"`\n" +
				"}\n"
			DefaultLogger.Errorf("%s", eg)
		}

		for _, msg := range registryWarnMessages {
			DefaultLogger.Errorf("%s", msg)
		}
		if len(registryWarnMessages) > 0 {
			DefaultLogger.Errorf("====================Operator Registry Warn End======================\n")
		}
	})
}

type OperatorField struct {
	reflect.StructField
	IsInput  bool
	IsOutput bool
	IsExtern bool
	IsMap    bool
	IsParam  bool

	IsSpan    bool
	IsContext bool

	AutoClear bool

	OrigInputType reflect.Type
	MapValueType  reflect.Type
}

type OperatorRegistry struct {
	Input  map[string]*OperatorField
	Output map[string]*OperatorField
	Params []*OperatorField

	Context *OperatorField
	Span    *OperatorField

	// Creator func() operatorInstance
	pool sync.Pool

	meta OperatorInfo
}

func (r *OperatorRegistry) GetOperator() *OperatorInstance {
	return r.pool.Get().(*OperatorInstance)
}

func (r *OperatorRegistry) resetInputs(op *OperatorInstance) {
	// clear input ptr
	for _, input := range r.Input {
		if !input.IsMap {
			fieldPtr := op.ptr + input.StructField.Offset
			switch input.Type.Kind() {
			case reflect.Map:
				fallthrough
			case reflect.Pointer:
				fallthrough
			case reflect.Interface:
				*(*uintptr)(unsafe.Pointer(fieldPtr)) = 0
			case reflect.Slice:
				slice := (*reflect.SliceHeader)(unsafe.Pointer(fieldPtr))
				slice.Len = 0
			}
		}
	}
	if nil != r.Context {
		fieldPtr := op.ptr + r.Context.StructField.Offset
		*(*uintptr)(unsafe.Pointer(fieldPtr)) = 0
	}
}

func (r *OperatorRegistry) resetOutputs(op *OperatorInstance) {
	// clear output

	for _, output := range r.Output {
		fieldPtr := op.ptr + output.StructField.Offset
		switch output.Type.Kind() {
		case reflect.Bool:
			*(*bool)(unsafe.Pointer(fieldPtr)) = false
		case reflect.String:
			*(*string)(unsafe.Pointer(fieldPtr)) = ""
		case reflect.Int:
			*(*int)(unsafe.Pointer(fieldPtr)) = 0
		case reflect.Int8:
			*(*int8)(unsafe.Pointer(fieldPtr)) = 0
		case reflect.Int16:
			*(*int16)(unsafe.Pointer(fieldPtr)) = 0
		case reflect.Int32:
			*(*int32)(unsafe.Pointer(fieldPtr)) = 0
		case reflect.Int64:
			*(*int64)(unsafe.Pointer(fieldPtr)) = 0
		case reflect.Uint8:
			*(*uint8)(unsafe.Pointer(fieldPtr)) = 0
		case reflect.Uint16:
			*(*uint16)(unsafe.Pointer(fieldPtr)) = 0
		case reflect.Uint32:
			*(*uint32)(unsafe.Pointer(fieldPtr)) = 0
		case reflect.Uint64:
			*(*uint64)(unsafe.Pointer(fieldPtr)) = 0
		case reflect.Float64:
			*(*float64)(unsafe.Pointer(fieldPtr)) = 0.0
		case reflect.Float32:
			*(*float32)(unsafe.Pointer(fieldPtr)) = 0.0
		case reflect.Slice:
			slice := (*reflect.SliceHeader)(unsafe.Pointer(fieldPtr))
			slice.Len = 0
		case reflect.Map:
			*(*uintptr)(unsafe.Pointer(fieldPtr)) = uintptr(0)
		case reflect.UnsafePointer:
			*(*uintptr)(unsafe.Pointer(fieldPtr)) = 0
		case reflect.Pointer:
			fallthrough
		case reflect.Interface:
			*(*uintptr)(unsafe.Pointer(fieldPtr)) = 0
		default:
			if output.AutoClear {
				val := reflect.NewAt(output.Type, unsafe.Pointer(fieldPtr)).Elem()
				val.SetZero()
			}
		}
	}
}

// func (r *OperatorRegistry) RecycleOperator(op *OperatorInstance) {
// 	r.resetInputs(op)
// 	op.op.Reset()
// 	r.pool.Put(op)
// }

var ops = make(map[string]*OperatorRegistry)

// RegisterOperator 注册算子
func RegisterOperator(name string, op Operator) error {
	_, srcFile, line, _ := runtime.Caller(1)

	if _, exist := ops[name]; exist {
		return ErrDuplicateName
	}
	t := reflect.TypeOf(op).Elem()
	registry := &OperatorRegistry{
		Input:  make(map[string]*OperatorField),
		Output: make(map[string]*OperatorField),
		pool: sync.Pool{
			New: func() interface{} {
				val := reflect.New(t)
				opInstance := val.Interface().(Operator)
				opInstance.Reset()
				opPtr := val.UnsafePointer()
				return &OperatorInstance{Name: name,
					op:  opInstance,
					ptr: uintptr(opPtr)}
			},
		},
	}

	var warnOutputNames []string

	for i := 0; i < t.NumField(); i++ {
		f := t.Field(i)

		// accept didagle/dag
		meta, ok := f.Tag.Lookup("didagle")
		if !ok {
			meta, ok = f.Tag.Lookup("dag")
			if !ok {
				continue
			}
		}
		if len(meta) == 0 {
			return ErrMissingStructTag
		}
		field := &OperatorField{
			StructField: f,
		}
		ss := strings.Split(meta, ",")
		hasContexField := false
		hasSpanField := false
		for _, s := range ss {
			switch tag := strings.ToLower(strings.TrimSpace(s)); tag {
			case "input":
				field.IsInput = true
			case "output":
				field.IsOutput = true
			case "extern":
				field.IsExtern = true
			case "map":
				field.IsMap = true
			case "param":
				field.IsParam = true
			case "auto_clear":
				field.AutoClear = true
			case "context":
				if hasContexField {
					DefaultLogger.Errorf("[%v] is context field, but already have context field in operator:%v", field.Name, name)
					return ErrInvalidStructTag
				}
				if field.IsInput || field.IsOutput || field.IsMap || field.IsParam {
					DefaultLogger.Errorf("[%v/%v] is context field, can not be other type like input/output/param.", name, field.Name)
					return ErrInvalidStructTag
				}
				field.IsContext = true
				hasContexField = true
			case "span":
				if hasSpanField {
					DefaultLogger.Errorf("[%v] is span field, but already have span field in operator:%v", field.Name, name)
					return ErrInvalidStructTag
				}
				if field.IsInput || field.IsOutput || field.IsMap || field.IsParam {
					DefaultLogger.Errorf("[%v/%v] is span field, can not be other type like input/output/param.", name, field.Name)
					return ErrInvalidStructTag
				}
				field.IsSpan = true
				hasSpanField = true
			default:
				return ErrInvalidStructTag
			}
		}
		if !field.IsInput && !field.IsOutput && !field.IsParam && !field.IsContext && !field.IsSpan {
			DefaultLogger.Errorf("[%v/%v] is not valid field, it should config with type like input/output/param.", name, field.Name)
			return ErrInvalidStructTag
		}

		if field.IsInput {
			if !field.IsMap {
				if field.Type.Kind() != reflect.Ptr && field.Type.Kind() != reflect.Interface && field.Type.Kind() != reflect.Map && field.Type.Kind() != reflect.Slice {
					return ErrInvalidInputFieldType
				}
				if field.Type.Kind() == reflect.Ptr {
					field.OrigInputType = field.Type.Elem()
				} else {
					field.OrigInputType = field.Type
				}
			} else {
				if field.Type.Kind() != reflect.Map {
					return ErrInvalidInputFieldType
				}
				keyType := field.Type.Key()
				if keyType.Kind() != reflect.String {
					return ErrInvalidInputFieldType
				}
				valType := field.Type.Elem()
				if valType.Kind() != reflect.Ptr {
					return ErrInvalidInputFieldType
				}
				field.MapValueType = valType
				field.OrigInputType = valType.Elem()
				DefaultLogger.Errorf("[%v] is map, orig input type:%v", field.Name, field.OrigInputType)
			}
			registry.Input[field.Name] = field
		}
		if field.IsOutput {
			registry.Output[field.Name] = field
			switch field.Type.Kind() {
			case reflect.Struct:
				fallthrough
			case reflect.Complex64:
				fallthrough
			case reflect.Complex128:
				fallthrough
			case reflect.Func:
				fallthrough
			case reflect.Chan:
				fallthrough
			case reflect.Array:
				if !field.AutoClear {
					warnOutputNames = append(warnOutputNames, field.Name)
				}
			default:
				// do nothing
			}
		}
		if len(warnOutputNames) > 0 {
			registryWarnMessages = append(registryWarnMessages, fmt.Sprintf("Operator:%s at %s:%v has fields:%v with complex type(struct/array/func/chan), which need to add reset operations for faster clear in the 'Reset' func or mark 'auto_clear' in struct tag.", name, srcFile, line, warnOutputNames))
		}

		if field.IsParam {
			registry.Params = append(registry.Params, field)
			if field.Type.Kind() == reflect.Slice {
				field.OrigInputType = field.Type.Elem()
			}
		}
		if field.IsContext {
			expectType := reflect.TypeOf((*ExecuteContext)(nil))
			if field.Type != expectType {
				DefaultLogger.Errorf("[%v/%v] is not valid field, it should define with type *didagle.ExecuteContext", name, field.Name)
				return ErrInvalidContextFieldType
			}
			registry.Context = field
		}
		if field.IsSpan {
			expectType := reflect.TypeOf((*Span)(nil)).Elem()
			if field.Type != expectType {
				DefaultLogger.Errorf("[%v/%v] is not valid field, it should define with type didagle.Span, but it's %v", name, field.Name, field.Type)
				return ErrInvalidContextFieldType
			}
			registry.Span = field
		}
	}
	registry.meta.Name = name
	for _, input := range registry.Input {
		fullTypeName := input.OrigInputType.Name()
		if len(input.OrigInputType.PkgPath()) > 0 {
			fullTypeName = input.OrigInputType.PkgPath() + "/" + fullTypeName
		}

		field := FieldInfo{
			Name: input.Name,
			ID:   int64(input.OrigInputType.Kind()),
			Type: fullTypeName,
		}
		if input.IsExtern {
			field.Flags.Extern = 1
		}
		if input.IsInput && input.IsOutput {
			field.Flags.InOut = 1
		}
		if input.IsMap {
			field.Flags.Agrregate = 1
		}
		registry.meta.Input = append(registry.meta.Input, field)
	}
	for _, output := range registry.Output {
		outputType := output.Type
		var fullTypeName string

		if outputType.Kind() == reflect.Pointer {
			outputType = outputType.Elem()
		}
		fullTypeName = outputType.Name()
		if len(outputType.PkgPath()) > 0 {
			fullTypeName = outputType.PkgPath() + "/" + fullTypeName
		}

		field := FieldInfo{
			Name: output.Name,
			ID:   int64(output.Type.Kind()),
			Type: fullTypeName,
		}
		if output.IsExtern {
			field.Flags.Extern = 1
		}
		if output.IsInput && output.IsOutput {
			field.Flags.InOut = 1
		}
		if output.IsMap {
			field.Flags.Agrregate = 1
		}
		registry.meta.Output = append(registry.meta.Output, field)
	}
	for _, param := range registry.Params {
		field := ParamInfo{
			Name: param.Name,
			Type: param.Type.Name(),
		}
		registry.meta.Params = append(registry.meta.Params, field)
	}
	registry.meta.Source = srcFile
	registry.meta.Line = line
	ops[name] = registry
	return nil
}

func GetOperator(name string) (*OperatorRegistry, error) {
	registry, exist := ops[name]
	if !exist {
		return nil, fmt.Errorf("operator not found for %v", name)
	}
	return registry, nil
}

func DumpAllOperatorMetaInfo() string {
	var metas []OperatorInfo
	for _, reg := range ops {
		metas = append(metas, reg.meta)
	}
	b, _ := json.MarshalIndent(metas, "", "    ")
	return string(b)
}
