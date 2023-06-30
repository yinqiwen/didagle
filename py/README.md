## pydidagle

### 安装
```shell
 pip3 install git+https://git.woa.com/lm-engine/pydidagle.git#egg=version_subpkg&subdirectory=py
```

### Python执行流

可将以下执行流代码文本发送到appengine server执行；也可在本地终端调试执行
```python
import pydidagle as dag

def test_g(block):
    block.execute("noop")
    block.execute("noop")
    # block.execute(func=test1.test_g2)
    with dag.new_while(block, "ARGS.loopIdx<3", async_exec=False) as while_block:
        while_block.execute("op4")


app = dag.new_block()
# 默认sequential=True表示串行逻辑
app.execute("op0", args={"testStrArg": "hello,world"})

# sequential=False表示sub_block内尽量并行执行，默认是串行逻辑
with dag.new_block(app, sequential=False) as sub_block:
    sub_block.execute("noop")
    sub_block.execute("noop")

# if block， if块
with dag.new_if(app, "ARGS.flag==1") as if_block:
    if_block.execute("op1")
# else block， else块
with dag.new_else(if_block) as else_block:
    else_block.execute("op2")

app.execute("op3")

# while block,循环块
with dag.new_while(app, "ARGS.loopIdx<10", async_exec=False) as while_block:
    while_block.execute("op4")

with dag.new_while(app, "ARGS.loopIdx<2", async_exec=False) as while_block:
    while_block.execute("op4")

# 调用方法
app.execute(func=test_g)
app.execute(func=test_g)

app.eval()

```

### 生成toml以及png图
生成png
```shell
python <dag.py> --meta_file <META_JSON_PATH> --png
```
生成toml
```shell
python <dag.py> --meta_file <META_JSON_PATH> --toml
```

同时生成toml和png
```shell
python <dag.py> --meta_file <META_JSON_PATH> --toml --png
```

### 远程调试
无参数
```shell
python <dag.py> -e -s 127.0.0.1:8000 
```
带json参数
```shell
python <dag.py> -e -s 127.0.0.1:8000 --args {\"flag\":1}
```

输出调试结果原始json
```shell
python <dag.py> -e -s 127.0.0.1:8000 --json_output 
```