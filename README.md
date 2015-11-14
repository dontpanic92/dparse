dparse
======

哈工大编译原理实验语法分析器，附带一个简易C语言后端。

词法分析器有独立版本，请见https://github.com/dontpanic92/dlex


dparse结构
======
dparse为LR(1)型语法分析器，采用语法制导定义方式进行代码生成。建立状态转移表的算法描述请参考龙书，代码基本上是完全按照龙书的描述实现的。

daprse前端语法分析部分由C++编写，三地址码生成及汇编代码生成部分使用lua编写。parser在lua解释环境中注入了两个函数，分别为`parser_getstackcontent`和`parser_getcurrenttokenname`，分别用于获取当前栈空间中的内容以及获取当前token的名字（用于处理identifier）。


dparse文件语法
======
dparse文件描述了语言的产生式，使用行开头#号作为注释。

文件的开头两行需要指定词法分析文件以及负责代码生成的lua程序，例如

```
/home/dontpanic/1.dlex
/home/dontpanic/1.lua
```

每一条产生式以冒号:开头，后跟产生式左端名称。

新起一行用空格分割产生式的各个部分。可以使用的符号包括产生式左端名称、以及由dlex文件定义的终结符名称。产生式后跟处理这条产生式的lua函数，使用冒号:与产生式隔开。

最后一行使用分号;结束这条产生式的定义。

例如

```
: valid-parse-structure
	declaration : process_declaration
;
```

上述代码定义了一个产生式`valid-parse-structure -> declaration`，并由`process_declaration`函数进行处理。

当多条产生式具有相同的左端时，它们需要写在一起。例如

```
: valid-parse-structure
	valid-parse-structure valid-parse-structure : process_valid_parse_structure
	declaration : process_declaration
	statement : process_statement
	function-definition : process_helper_directcopy
;
```
定义了4条产生式，分别为

```
valid-parse-structure -> valid-parse-structure valid-parse-structure
valid-parse-structure -> declaration
valid-parse-structure -> statement
valid-parse-structure -> function-definition
```

更详细的例子请参考example/1.dparse。
