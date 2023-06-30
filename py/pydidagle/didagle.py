
import json
import sys
import os
from collections import defaultdict
from inspect import getframeinfo, stack
from typing import Optional
from typing import Final
from typing import List

from dataclasses import dataclass
from dataclasses_json import dataclass_json
import toml
import graphviz
import requests


_block_id_counter = 0
_node_id_counter = 0
_gengraph_id_counter = 0

EXPR_OP_NAME: Final[str] = "ispine_didagle_expr"
WHILE_EXPR_OP_NAME: Final[str] = "ispine_didagle_while"
EXPR_PARAM_KEY: Final[str] = "__expr"

run_args = None

# def createClass(classname, attributes):
#     return type(classname, (object,), attributes)


@dataclass_json
@dataclass
class IODataFlags:
    is_extern: int
    is_in_out: int
    is_aggregate: int


@dataclass_json
@dataclass
class IOData:
    id: int
    name: str
    type: str
    flags: IODataFlags


@dataclass_json
@dataclass
class Param:
    name: str
    type: str


@dataclass_json
@dataclass
class Operator:
    name: str
    source: str
    line: int
    input: Optional[List[IOData]] = None
    output: Optional[List[IOData]] = None
    params: Optional[List[Param]] = None


operator_meta_table = {}
global_func_block = {}


def abort(msg: str):
    sys.exit(msg)


@dataclass
class SelectArg:
    """Basic example class"""
    match: str
    args: dict


class NodeField:
    def __init__(self,  valDict):
        self.extern = False
        self.id = ""
        self.aggregate = []
        self.required = False
        for k, v in valDict.items():
            if not hasattr(self, k):
                abort("No field " + k + " defined!!!")
            if isinstance(k, (list, tuple)):
                setattr(self, k, [obj(x) if isinstance(
                    x, dict) else x for x in v])
            else:
                setattr(self, k, obj(v) if isinstance(v, dict) else v)


class Node:
    def __init__(self, op: Operator = None, block: 'Block' = None, start=False, expect: str = "", args=None,
                 select_args: list[SelectArg] = None, input=None, output=None, is_if=False, is_while=False, subgraph="", async_exec=False, caller=None):
        global _node_id_counter
        self.block = block
        self.op = op
        self.start = start
        self.id = _node_id_counter
        self.successor = {}
        self.successor_on_ok = {}
        self.successor_on_err = {}
        if args is None:
            args = {}
        self.args = args
        if select_args is None:
            select_args = []
        self.select_args = select_args
        self.subgraph = subgraph
        self.async_exec = async_exec
        if input is None:
            input = {}
        self.input = {}
        if output is None:
            output = {}
        self.output = {}
        for k, v in input.items():
            self.input[k] = NodeField(valDict=v)
        for k, v in output.items():
            self.output[k] = NodeField(valDict=v)
        self.is_if = is_if
        self.is_while = is_while
        self.caller = caller

        self.deps = set()
        _node_id_counter = _node_id_counter+1

    def __str__(self):
        if self.op is not None:
            return "Operator:" + self.op.name
        if self.block is not None:
            s = "Block:"
            for n in self.block.nodes:
                s += "\n\t" + str(n)
            return s

    def _has_successor(self, id, except_self=False):
        if except_self is False:
            if id in self.successor:
                return True
            if id in self.successor_on_ok:
                return True
            if id in self.successor_on_err:
                return True
        for _, node in self.successor.items():
            if node._has_successor(id):
                return True
        for _, node in self.successor_on_ok.items():
            if node._has_successor(id):
                return True
        for _, node in self.successor_on_err.items():
            if node._has_successor(id):
                return True
        return False

    def _add_successor(self, node):
        if self.is_if:
            for __, successor_node in self.successor.items():
                successor_node._add_successor(node)
            for __, successor_node in self.successor_on_ok.items():
                successor_node._add_successor(node)
            for __, successor_node in self.successor_on_err.items():
                successor_node._add_successor(node)
        else:
            self.successor[node.id] = node

    def _add_successor_on_ok(self, node):
        self.successor_on_ok[node.id] = node

    def _add_successor_on_err(self, node):
        self.successor_on_err[node.id] = node

    def _remove_successor(self, node):
        del self.successor[node.id]

    def _extract_block_successor(self):
        del_successor = []
        for id, node in self.successor.items():
            if node.block is not None and not node.block.is_func:
                nodes_dict = {}
                node.block._get_nodes(nodes_dict)
                self.successor = self.successor | nodes_dict
                del_successor.append(id)
        for id in del_successor:
            del self.successor[id]
        del_successor_on_ok = []
        for id, node in self.successor_on_ok.items():
            if node.block is not None and not node.block.is_func:
                nodes_dict = {}
                node.block._get_nodes(nodes_dict)
                self.successor_on_ok = self.successor_on_ok | nodes_dict
                del_successor_on_ok.append(id)
        for id in del_successor_on_ok:
            del self.del_successor_on_ok[id]
        del_successor_on_err = []
        for id, node in self.successor_on_err.items():
            if node.block is not None and not node.block.is_func:
                nodes_dict = {}
                node.block._get_nodes(nodes_dict)
                self.successor_on_err = self.successor_on_err | nodes_dict
                del_successor_on_err.append(id)
        for id in del_successor_on_err:
            del self.successor_on_err[id]

        if self.block is not None and not self.block.is_func:
            nodes_dict = {}
            self.block._get_nodes(nodes_dict)
            for id, node in nodes_dict.items():
                node.successor = node.successor | self.successor
                node.successor_on_ok = node.successor_on_ok | self.successor_on_ok
                node.successor_on_err = node.successor_on_err | self.successor_on_err

    def _mark_input_extern(self, extern_input_key):
        input_table = {}
        if self.op.input is not None:
            for input in self.op.input:
                input_key = input.name
                is_extern = input.flags.is_extern
                if input.name in self.input:
                    input_reg = self.input[input.name]
                    if len(input_reg.id) > 0:
                        input_key = input_reg.id
                    if input_reg.extern:
                        is_extern = 1
                if is_extern == 0:
                    input_key = input_key + ":" + input.type
                    input_table[input_key] = input.name
        if extern_input_key in input_table:
            input_name = input_table[extern_input_key]
            if input_name in self.input:
                self.input[input_name].extern = True
            else:
                self.input[input_name] = NodeField(valDict={})
                self.input[input_name].extern = True

    def _collect_input_output(self, input_table, output_table):
        if self.op.input is not None:
            for input in self.op.input:
                input_key = input.name
                is_extern = input.flags.is_extern
                if input.name in self.input:
                    input_reg = self.input[input.name]
                    if len(input_reg.id) > 0:
                        input_key = input_reg.id
                    if input_reg.extern:
                        is_extern = 1
                if is_extern == 0:
                    input_table[input_key + ":" + input.type].add(self)

        if self.op.output is not None:
            for output in self.op.output:
                output_key = output.name
                if output.name in self.output:
                    output_reg = self.output[output.name]
                    if len(output_reg.id) > 0:
                        output_key = output_reg.id
                output_key = output_key + ":" + output.type
                if output_key in output_table:
                    abort("Duplicate output id:" + output_key +
                          " in operator:" + str(self))
                output_table[output_key] = self


class Block:
    def __init__(self, parent: 'Block' = None, name="", is_func=False, sequential=True):
        global _block_id_counter
        self.parent = parent
        self.nodes = []
        self.id = _block_id_counter
        self.sequential_exec = sequential
        self.subgraph = {}
        self.name = name
        self.is_func = is_func
        self.dot_lable = ""
        self.build_node_dict = {}
        if self.parent is None and len(name) == 0:
            self.name = "main"
        _block_id_counter = _block_id_counter+1

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        if len(self.nodes) == 0:
            abort("Block has empty body!!!")
        if len(self.nodes) == 1 and self.is_main_func:
            if self.parent is not None:
                self.parent._add_node(self.nodes[0])
                return
        if self.parent is not None:
            blockNode = Node(block=self)
            self.parent._add_node(blockNode)
            if self.is_func:
                self.parent._add_subgraph(self)

    def _add_subgraph(self, g):
        if self.parent is None:
            self.subgraph[g.name] = g
        else:
            self.parent._add_subgraph(g)

    def _get_subgraph(self, name):
        if self.parent is None:
            return self.subgraph[name]
        else:
            return self.parent._get_subgraph(name)

    def _add_node(self, node: Node):
        self.nodes.append(node)
        if self.sequential_exec:
            if self.parent is None and len(self.nodes) == 1:
                node.start = True
            if len(self.nodes) > 1:
                self.nodes[-2]._add_successor(self.nodes[-1])

    def _get_nodes(self, nodes_dict):
        for node in self.nodes:
            nodes_dict[node.id] = node
            for __, child in node.successor.items():
                nodes_dict[child.id] = child
            for __, child in node.successor_on_ok.items():
                nodes_dict[child.id] = child
            for __, child in node.successor_on_err.items():
                nodes_dict[child.id] = child
            if node.block is not None and not node.block.is_func:
                node.block._get_nodes(nodes_dict)

    def execute(self, op: str = "", func=None, start=False, sequential=True, expect: str = "", args=None, select_args: list[SelectArg] = None, input=None, output=None):
        global operator_meta_table
        global global_func_block
        if len(op) == 0 and func is None:
            abort("Block:" + self.name +
                  " MUST execute execute non empty operateor or functor!!!")
        if len(op) > 0:
            if op not in operator_meta_table:
                abort(op + " is not valid operator!!!")
            caller = getframeinfo(stack()[1][0])
            node = Node(op=operator_meta_table[op], start=start, expect=expect, args=args,
                        select_args=select_args, input=input, output=output, caller=caller)
            self._add_node(node)
        else:
            name = func.__name__
            if func.__module__ != '__main__':
                name = func.__module__ + "." + name
            if name in global_func_block:
                block = global_func_block[name]
                blockNode = Node(block=block)
                self._add_node(blockNode)
            else:
                block = Block(self, name=name, is_func=True,
                              sequential=sequential)
                global_func_block[name] = block
                func(block)
                blockNode = Node(block=block)
                self._add_node(blockNode)
                self._add_subgraph(block)
                global_func_block[name] = block

    def _build_nodes_dict(self, output_table, extern_output_table):
        nodes_dict = {}
        if extern_output_table is None:
            extern_output_table = {}
        self. _get_nodes(nodes_dict)
        input_table = defaultdict(set)
        for id, node in nodes_dict.items():
            # Collect input/output table
            node._extract_block_successor()
            if node.op is not None:
                node._collect_input_output(input_table, output_table)
            for __, child in node.successor.items():
                nodes_dict[child.id].deps.add(self.id)
            for __, child in node.successor_on_ok.items():
                nodes_dict[child.id].deps.add(self.id)
            for __, child in node.successor_on_err.items():
                nodes_dict[child.id].deps.add(self.id)

        for input_key, input_nodes in input_table.items():
            if input_key not in output_table:
                if input_key not in extern_output_table:
                    abort(input_key + " can NOT found in all op's output")
                else:
                    for node in input_nodes:
                        node._mark_input_extern(input_key)
            else:
                for node in input_nodes:
                    output_table[input_key]._add_successor(node)

        for id, node in nodes_dict.items():
            if node.op is not None:
                if node._has_successor(id):
                    abort("Circle dependency exist for node:" + str(node))
                if len(node.deps) == 0 and len(node.successor) == 0 and len(node.successor_on_ok) == 0 and len(node.successor_on_err) == 0 and not node.start:
                    abort("node:" + str(node) + " has no deps or successors!!!")
        return nodes_dict

    def _build_toml_dict(self, nodes_dict):
        graph = {}
        graph["name"] = self.name
        graph["vertex"] = []
        for k in sorted(nodes_dict.keys()):
            node = nodes_dict[k]
            v = {}
            v["id"] = str(node.id)
            if node.start:
                v["start"] = True
            if node.async_exec:
                v["async"] = True
            if len(node.successor) > 0:
                v["successor"] = [str(node.id)
                                  for _, node in node.successor.items()]
            if len(node.successor_on_ok) > 0:
                v["if"] = [str(node.id)
                           for _, node in node.successor_on_ok.items()]
            if len(node.successor_on_err) > 0:
                v["else"] = [str(node.id)
                             for _, node in node.successor_on_err.items()]
            if len(node.args) > 0:
                v["args"] = node.args
            if len(node.select_args) > 0:
                v["select_args"] = node.select_args

            if node.block is not None and node.block.is_func:
                v["graph"] = node.block.name
            if node.op is not None:
                if node.op.name != WHILE_EXPR_OP_NAME:
                    v["processor"] = node.op.name
                if node.is_if:
                    v["cond"] = node.args[EXPR_PARAM_KEY]
                    del node.args[EXPR_PARAM_KEY]
                if node.is_while:
                    v["while"] = node.args[EXPR_PARAM_KEY]
                    del node.args[EXPR_PARAM_KEY]

                if len(node.subgraph) > 0:
                    v["graph"] = node.subgraph

                if len(node.input) > 0:
                    v["input"] = []
                    for key, input in node.input.items():
                        custom_in = {}
                        custom_in["field"] = key
                        if input.extern:
                            custom_in["extern"] = True
                        if input.required:
                            custom_in["required"] = True
                        if len(input.id) > 0:
                            custom_in["id"] = input.id
                        if len(input.aggregate) > 0:
                            custom_in["aggregate"] = input.aggregate
                        v["input"].append(custom_in)

            graph["vertex"].append(v)
        return graph

    def _build_dot(self, g, c, nodes_dict):
        for k in sorted(nodes_dict.keys()):
            node = nodes_dict[k]
            if node.block is not None:
                if node.block.is_func:
                    c.node(str(node.id), node.block.name, shape='box', )
            elif node.is_while:
                c.node(str(node.id), 'while(' +
                       node.args["__expr"] + ")", shape='diamond', )
            elif node.is_if:
                c.node(str(node.id),
                       'if(' + node.args["__expr"] + ")", shape='diamond',)
            else:
                c.node(str(node.id), node.op.name)
        for k in sorted(nodes_dict.keys()):
            node = nodes_dict[k]
            if node.block is not None and not node.block.is_func:
                continue
            edge_label = None
            if node.is_while:
                edge_label = 'exit_while'
            for k, child in node.successor.items():
                if node._has_successor(child.id, except_self=True):
                    pass
                else:
                    c.edge(str(node.id), str(child.id), label=edge_label)
            for k, child in node.successor_on_ok.items():
                if node._has_successor(child.id, except_self=True):
                    pass
                else:
                    if node.is_if:
                        c.edge(str(node.id), str(child.id),
                               label="if", style='bold')
                    else:
                        c.edge(str(node.id), str(child.id), label=edge_label)
            for k, child in node.successor_on_err.items():
                if node._has_successor(child.id, except_self=True):
                    pass
                else:
                    if node.is_if:
                        c.edge(str(node.id), str(child.id),
                               label="else", style='dashed', color='red')
                    else:
                        c.edge(str(node.id), str(child.id), label=edge_label)

    def _dot_graph(self, g, cluster_name, extern_output_table):
        if len(self.nodes) == 1:
            self.nodes[0].start = True
        node_dict = self._build_nodes_dict({}, extern_output_table)
        with g.subgraph(name=cluster_name) as c:
            c.attr(color='blue', style='rounded')
            c.node_attr.update(style='filled', fillcolor='lightcyan')
            c.attr(label=self.name)
            self._build_dot(g, c, node_dict)
        return node_dict

    def _dot_connect_other_gprah(self, g, nodes_dict):
        for k in sorted(nodes_dict.keys()):
            node = nodes_dict[k]
            if node.is_while or (node.block is not None and node.block.is_func):
                if node.is_while:
                    subgraph = self._get_subgraph(node.subgraph)
                    g.edge(str(node.id), str(
                        subgraph.nodes[0].id), label='loop', lhead=subgraph.dot_lable)
                else:
                    subgraph = self._get_subgraph(node.block.name)
                    g.edge(str(node.id), str(
                        subgraph.nodes[0].id), label='invoke', lhead=subgraph.dot_lable)
                g.edge(str(subgraph.nodes[0].id), str(
                    node.id), ltail=subgraph.dot_lable, style="dashed")

    def eval(self):
        main_output_table = {}
        main_nodes_dict = self._build_nodes_dict(main_output_table, {})
        dump_dot = False
        dump_toml = False
        if not run_args.evaluate:
            dump_dot = run_args.png
            dump_toml = run_args.toml
        if dump_dot:
            g = graphviz.Digraph('G')
            g.attr(compound='true')
            g.node('START', 'START', shape='Mcircle',
                   style='filled', fillcolor='skyblue')
            g.node('END', 'END', shape='Mcircle',
                   style='filled', fillcolor='skyblue')
            with g.subgraph(name='cluster_0') as c:
                c.attr(style='filled', color='lightgrey')
                c.node_attr.update(style='filled', fillcolor='skyblue')
                c.attr(label='main')
                self._build_dot(g, c, main_nodes_dict)
            cluster_idx = 1
            for __, gblock in self.subgraph.items():
                cluster_name = 'cluster_' + str(cluster_idx)
                gblock.dot_lable = cluster_name
                cluster_idx = cluster_idx+1
                gblock.build_node_dict = gblock._dot_graph(
                    g, cluster_name, main_output_table)

            self._dot_connect_other_gprah(g, main_nodes_dict)
            for __, gblock in self.subgraph.items():
                gblock._dot_connect_other_gprah(g, gblock.build_node_dict)

            for k in sorted(main_nodes_dict.keys()):
                node = main_nodes_dict[k]
                if node.start:
                    g.edge('START', str(node.id))
                if len(node.successor) == 0 and len(node.successor_on_ok) == 0 and len(node.successor_on_err) == 0:
                    g.edge(str(node.id), 'END')
                # if node.is_while or (node.block is not None and node.block.is_func):
                #     for __, subgraph in self.subgraph.items():
                #         match = False
                #         if subgraph.name == node.subgraph:
                #             match = True
                #             g.edge(str(node.id), str(
                #                 subgraph.nodes[0].id), label='loop', lhead=subgraph.dot_lable)
                #         if node.block is not None and node.block.is_func and node.block.name == subgraph.name:
                #             match = True
                #             g.edge(str(node.id), str(
                #                 subgraph.nodes[0].id), label='invoke', lhead=subgraph.dot_lable)
                #         if match:
                #             g.edge(str(subgraph.nodes[0].id), str(
                #                 node.id), ltail=subgraph.dot_lable)
            png_file = os.path.basename(sys.argv[0]) + ".png"
            with open(png_file, "wb") as binary_file:
                binary_file.write(graphviz.Source(
                    g.source, format='png').pipe())
        if dump_toml:
            toml_dict = {}
            toml_dict["graph"] = [self._build_toml_dict(main_nodes_dict)]
            for __, g in self.subgraph.items():
                if len(g.nodes) == 1:
                    g.nodes[0].start = True
                subgraph_node_dict = g._build_nodes_dict({}, main_output_table)
                toml_dict["graph"].append(
                    g._build_toml_dict(subgraph_node_dict))
            toml_file = os.path.basename(sys.argv[0]) + ".toml"
            with open(toml_file, "w") as tfile:
                toml.dump(toml_dict, tfile)
        if run_args.evaluate:
            with open(sys.argv[0], 'r') as file:
                script = file.read()
                url = 'http://' + \
                    os.getenv('PYDIDAGLE_SERVER') + '/appengine/eval'
                body = {}
                body['script_content'] = script
                body['enable_trace'] = True
                body['script_name'] = os.path.basename(sys.argv[0])
                if run_args.args is not None:
                    body['json_args'] = run_args.args
                res = requests.post(url, json=body)
                res_json = res.json()
                if run_args.json_output:
                    print(res_json)
                else:
                    if len(res_json.get('err_msg')) > 0:
                        print('Remote Server Error:\n', res_json['err_msg'])
                    else:
                        print('Trace:\n', res_json['trace_msg'])
                        print('Result:\n', res_json['json'])


class IfBlock(Block):
    def __init__(self, parent: Block, expr: str, sequential=True):
        super().__init__(parent, sequential=sequential)
        self.expr = expr
        self.if_expr_node = Node(op=operator_meta_table[EXPR_OP_NAME], args={
                                 "__expr": expr}, is_if=True)

    def __enter__(self):
        if self.parent is not None:
            self.parent._add_node(self.if_expr_node)
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        if len(self.nodes) == 0:
            abort("if:" + self.expr + " has empty body!!!")
        for node in self.nodes:
            self.if_expr_node._add_successor_on_ok(node)


class ElseBlock(Block):
    def __init__(self, if_block: IfBlock, sequential=True):
        super().__init__(if_block.parent, sequential=sequential)
        self.if_block = if_block

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        if len(self.nodes) == 0:
            abort("else:" + self.expr + " has empty body!!!")
        for node in self.nodes:
            self.if_block.if_expr_node._add_successor_on_err(node)


class WhileBlock(Block):
    def __init__(self, parent: Block, expr: str, sequential=True, async_exec=False):
        global _gengraph_id_counter
        gen_name = "while_" + str(_gengraph_id_counter)
        _gengraph_id_counter = _gengraph_id_counter + 1

        super().__init__(parent, name=gen_name, sequential=sequential)
        self.expr = expr
        self.while_node = Node(op=operator_meta_table[WHILE_EXPR_OP_NAME], args={
                               "__expr": expr}, subgraph=gen_name, is_while=True, async_exec=async_exec)

    def __enter__(self):
        if self.parent is not None:
            self.parent._add_node(self.while_node)
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        if len(self.nodes) == 0:
            abort("while:" + self.expr + " has empty body!!!")
        if self.parent is not None:
            self.parent._add_subgraph(self)


def load_operator_meta_json_content(json_content: str):
    global operator_meta_table
    # json_config=json.loads(config_contents)
    result = Operator.schema().loads(json_content, many=True)
    for op in result:
        operator_meta_table[op.name] = op
        # field_dict = {}
        # field_dict["line"] = op.line
        # field_dict["file"] = op.source
        # if op.input is not None:
        #     field_dict["input"] = {}
        #     for input in op.input:
        #         field_dict["input"][input.name] = NodeField()
        # if op.output is not None:
        #     field_dict["output"] = {}
        #     for output in op.output:
        #         field_dict["output"][output.name] = NodeField()
        # createClass(op.name,field_dict)


def load_operator_meta_json(json_file: str):
    with open(json_file) as jfile:
        config_contents = jfile.read()
    # json_config=json.loads(config_contents)
    load_operator_meta_json_content(config_contents)


def get_block_name(func_name):
    if func_name == '<module>':
        return 'main'
    else:
        return func_name


def new_block(parent: Block = None, sequential=True):
    func_name = get_block_name(
        getframeinfo(stack()[1][0]).function)
    return Block(parent, name=func_name, sequential=sequential)


def new_if(parent: Block, expr: str, sequential=True):
    return IfBlock(parent, expr, sequential)


def new_else(if_block: IfBlock, sequential=True):
    return ElseBlock(if_block, sequential)


def new_while(parent: Block, expr: str, sequential=True, async_exec=False):
    return WhileBlock(parent, expr, sequential, async_exec)
