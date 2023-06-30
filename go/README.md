## didagle

### 算子编写
算子为基本执行单元，用golang代码定义，包含如下元素：
- input成员变量，可以为指针/map/slice/interface，代表输入， 可定义多个；
- output成员变量，本质为成员对象， 可定义多个；注意复杂类型的output需要在Reset方法中增加clear实现，或者加上auto_clear标识（基于反射的自动clear，性能较差）
- param参数，支持bool/int/float/string等，可定义多个；
- 初始化Setup方法， 算子对象生命周期内调用一次，可以不用重载定义；
- 重置Reset方法， 算子对象生命周期内调用N次，执行引擎全任务执行完毕后执行，可以不用重载定义；
- 执行Execute方法，算子对象生命周期内调用N次

```golang
import (
	"context"

	didagle "github.com/yinqiwen/didagle/go"
)

type testOutStruct struct{
    V int
}

type testOp0 struct {
    dagRPCReq *helloworld.HelloRequest `didagle:"input,extern"`
    op0Input0 *string                  `didagle:"input"`
    op0Input1 map[string]int           `didagle:"input"`

    op0Result0 string                  `didagle:"output"`
    op0Result1 int                     `didagle:"output"`
    // 复杂类型的output需要在Reset方法中增加clear实现，或者加上auto_clear标识（基于反射的clear，性能较差）
    testOut    testOutStruct           `didagle:"output, auto_clear"`

    loopIdx   int       `didagle:"param"`
    intArgs   []int     `didagle:"param"`
    strArgs   []string  `didagle:"param"`
}

func (op *testOp0) Setup(args map[string]interface{}) error {
	return nil
}
func (op *testOp0) Reset() {
}
func (op *testOp0) Execute(ctx context.Context, args *didagle.Params) error {
	op.op0Result0 = op.dagRPCReq.Msg + ":testOp0"
    op.op0Result1 = 123
	return nil
}

func init() {
	_ = didagle.RegisterOperator("op0", new(testOp0))
}
```

