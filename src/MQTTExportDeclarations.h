/*******************************************************************************
 * Copyright (c) 2020, 2020 Andreas Walter
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v2.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    https://www.eclipse.org/legal/epl-2.0/
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Andreas Walter - initially moved export declarations into separate fle
 *******************************************************************************/

 // comment by Clark:: visibility 属性用于设置 动态链接库 中函数的可见性, 将变量或函数设置为 hidden, 则该符号仅在本 so 中可见，在其他库中则不可见. ::2020-12-21
// comment by Clark:: 编写大型程序时, 可用-fvisibility=hidden 设置符号默认隐藏, 针对特定变量和函数, 在代码中使用 __attribte__((visibility("default"))令该符号外部可见，这种方法可有效避免so之间的符号冲突  ::2020-12-21
// comment by Clark:: 在GCC 4.0及以上版本中, 有个visibility属性, 可见属性可应用到函数、变量、模板以及C++类。  ::2020-12-21
 // comment by Clark:: 限制符号可见性的原因: 从动态库中尽可能少地输出符号是一个好的实践经验.输出一个受限制的符号会提高程序的模块性, 并隐藏实现的细节。动态库装载和识别的符号越少, 程序启动和运行的速度就越快, 导出所有符号会减慢程序速度，并耗用大量内存  ::2020-12-21
/**
 * "default": 用它定义的符号将被导出, 动态库中的函数默认是可见的", 该方法或对其它模块是可见的
 * "hidden":  用它定义的符号将不被导出, 并且不能从其它对象进行使用, 动态库中的函数是被隐藏的.该方法符号不会放到动态符号表里，所以其它模块(可执行文件或者动态库)不可以通过符号表访问该方法, 要定义 GNU 属性，需要包含 __attribute__ 和用括号括住的内容。可以将符号的可见性
 * 指定为visibility("hidden"), 这将不允许它们在库中被导出，但是可以在源文件之间共享。实际上, 隐藏的符号将不会出现在动态符号表中，但是还被留在符号表中用于静态链接。
 * 导出列表由编译器在创建共享库的时候自动生成，也可以由开发人员手工编写, 导出列表的原理是显式地告诉编译器可以通过外部文件从对象文件导出的符号是哪些。GNU用户将此类外部文件称作为"导出映射"
*/

#if !defined(EXPORTDECLARATIONS_H)
#define EXPORTDECLARATIONS_H

#if defined(_WIN32) || defined(_WIN64)
#   if defined(PAHO_MQTT_EXPORTS)
#       define LIBMQTT_API __declspec(dllexport)
#   elif defined(PAHO_MQTT_IMPORTS)
#       define LIBMQTT_API __declspec(dllimport)
#   else
#       define LIBMQTT_API
#   endif
#else
#    if defined(PAHO_MQTT_EXPORTS)
#       define LIBMQTT_API  __attribute__ ((visibility ("default")))
#    else
#       define LIBMQTT_API extern
#    endif
#endif

#endif
