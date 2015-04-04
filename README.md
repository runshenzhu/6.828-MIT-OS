# 6.828-MIT-OS

##问题
1. 内核从0xf000000开始map，到底map了多少? 4M? page_init()函数中不需要init的gap到底多大？

2. mem_init()中 Map all of physical memory at KERNBASE. the VA range [KERNBASE, 2^32) should map to the PA range [0, 2^32 - KERNBASE) We might not have 2^32 - KERNBASE bytes of physical memory, but we just set up the mapping anyway. 什么意思? 这两段不是一样长的貌似。(linux中貌似有高地址的静态映射/动态映射什么乱七八糟的)
