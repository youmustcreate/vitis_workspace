#ifndef __PLATFORM_CONFIG_H_
#define __PLATFORM_CONFIG_H_
// 防止头文件被多次引入而造成重复定义
// _PLATFORM_CONFIG_H 是一个自定义的宏名。
// 当这个头文件被引入多次的时候,通过检查是否定义了__PLATFORM_CONFIG_H_宏,可以避免重复引入头文件定义。

// 定义 __PLATFORM_CONFIG_H_ 这样一个宏的主要目的是为了确保头文件只被包含一次，以防止编译时的重复定义错误和潜在的编译问题。
// 这是一种很常见的 C/C++ 编程实践，通常被称为头文件保护（Header Guard）或宏保护（Macro Guard）。

//当你编写一个头文件（例如 platform_config.h）时，这个头文件可能会被其他源文件多次包含。
// 如果没有头文件保护，每次包含都会导致头文件的内容被复制到包含它的源文件中。这可能会导致以下问题：

// 重复定义错误：如果头文件中包含了变量、函数或其他声明，多次定义相同的内容将导致编译器错误。
// 编译时间增加：多次包含相同的内容会增加编译时间，因为编译器需要处理相同的代码多次。
// 潜在问题：如果头文件中包含了条件编译指令（#ifdef、#ifndef 等），多次包含可能会导致不正确的条件判断，从而引入潜在的错误。


// 通过定义 __PLATFORM_CONFIG_H_ 这样一个宏，你可以确保头文件只在第一次包含时真正地被处理，而后续的包含操作会被预处理器忽略，从而避免了上述问题。
// 这个宏的名称通常是全大写的，以避免与其他宏冲突，但它可以根据需要自定义为其他名称。


#define PLATFORM_EMAC_BASEADDR XPAR_XEMACPS_0_BASEADDR

#define PLATFORM_ZYNQ 


#endif
