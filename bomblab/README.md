&emsp;&emsp;CS:APP Lab网站：<https://csapp.cs.cmu.edu/3e/labs.html>。

&emsp;&emsp;这个lab的本质是逆向工程，锻炼x86-64汇编代码的阅读能力和gdb调试器的使用。为了使gdb页面好看，我特地下了pwndbg插件，它支持所有的gdb命令，能够把很多debug信息显示出来，最主要是配色和界面是真心不错。

&emsp;&emsp;实验提供了bomb的main函数源码，让你大概知道整个可执行文件的执行流程和逻辑。

&emsp;&emsp;本人实验环境：
- Ubuntu 22.04 虚拟机
- GNU gdb 12.1
- GNU objdump 2.38

&emsp;&emsp;使用`objdump`工具，执行命令：

```shell
$ objdump -d bomb > bomb.asm
```

将相应汇编代码输出至`bomb.asm`文件。

# phase 1
&emsp;&emsp;由于每调用一个phase函数，都会将之前终端输入的字符串`input`作为惟一的参数传入，其地址自然被放在了第一参数寄存器`%rdi`上。

&emsp;&emsp;`phase_1`函数旋即将一个地址`0x402400`放入了第二参数寄存器`%rsi`，然后调用了一个叫做`strings_not_equal`提示性极强的函数。最后检查返回值`%eax`，如果为0就成功，否则...蹦蹦炸弹！！！

&emsp;&emsp;基本上可以猜到，这个phase要求输入字符串和程序内部某个常量字符串一致。

&emsp;&emsp;gdb检查相应位置的字符串：

```shell
pwndbg> x/s 0x402400
0x402400:	"Border relations with Canada have never been better."
```
&emsp;&emsp;好了，这就是第一个bomb的key。

# phase 2
&emsp;&emsp;`read_six_numbers`提示我们这回是输入六个数字。随便输入`1 2 3 4 5 6`，看看堆栈上面的内存情况：
```shell
───────────────────────────────────[ STACK ]────────────────────────────────────
00:0000│ rsp 0x7fffffffde70 ◂— 0x200000001
01:0008│     0x7fffffffde78 ◂— 0x400000003
02:0010│     0x7fffffffde80 ◂— 0x600000005
03:0018│     0x7fffffffde88 —▸ 0x401431 (skip+56) ◂— test eax, eax
04:0020│     0x7fffffffde90 ◂— 0x0
05:0028│     0x7fffffffde98 ◂— 0x0
06:0030│     0x7fffffffdea0 ◂— 0x1
07:0038│     0x7fffffffdea8 —▸ 0x400e5b (main+187) ◂— call 0x4015c4

```

&emsp;&emsp;可以看到六个数字以4字节为大小在堆栈上按地址增序排列。

&emsp;&emsp;随机代码检查我们输入的第一个数，如果不是1就爆炸。

```x86asm
 400f0a:	83 3c 24 01          	cmpl   $0x1,(%rsp)
  400f0e:	74 20                	je     400f30 <phase_2+0x34>
  400f10:	e8 25 05 00 00       	call   40143a <explode_bomb>
```

&emsp;&emsp;接下来代码会进入一个循环：
```x86asm
 400f17:	8b 43 fc             	mov    -0x4(%rbx),%eax
  400f1a:	01 c0                	add    %eax,%eax
  400f1c:	39 03                	cmp    %eax,(%rbx)
  400f1e:	74 05                	je     400f25 <phase_2+0x29>
  400f20:	e8 15 05 00 00       	call   40143a <explode_bomb>
  400f25:	48 83 c3 04          	add    $0x4,%rbx
  400f29:	48 39 eb             	cmp    %rbp,%rbx
  400f2c:	75 e9                	jne    400f17 <phase_2+0x1b>
  400f2e:	eb 0c                	jmp    400f3c <phase_2+0x40>
->400f30:	48 8d 5c 24 04       	lea    0x4(%rsp),%rbx
  400f35:	48 8d 6c 24 18       	lea    0x18(%rsp),%rbp
  400f3a:	eb db                	jmp    400f17 <phase_2+0x1b>
```

&emsp;&emsp;6个数字可以看成一个长为6的数组$a[0..5]$，首先给`%rbx`写上$a[1]$的地址，再给`%rbp`写上数组尾巴$a[6]$的地址，随后对每个数遍历处理，循环结束。

&emsp;&emsp;这个循环的核心逻辑在于：$\forall i \in [1, 5], a[i] = 2*a[i-1]$：

```x86asm
 400f17:	8b 43 fc             	mov    -0x4(%rbx),%eax          ; a[i-1]
  400f1a:	01 c0                	add    %eax,%eax                ;2 * a[i-1]
  400f1c:	39 03                	cmp    %eax,(%rbx)              ;a[i] = 2*a[i-1]
  400f1e:	74 05                	je     400f25 <phase_2+0x29>
  400f20:	e8 15 05 00 00       	call   40143a <explode_bomb>
```

&emsp;&emsp;即后一个数是前一个数的两倍。结合前面得知的第一个数为1的事实，不难得到这个bomb的key为：`1 2 4 8 16 32`。

# phase 3
&emsp;&emsp;检查一下`0x4025cf`处保存的内容：
```shell
pwndbg> x/s 0x4025cf
0x4025cf:	"%d %d"
```

&emsp;&emsp;于是我们知道`phase_3`首先做了如下调用：

```c
int rsp_8, rsp_c;
int* rdx = &rsp_8, *rcx = &rsp_c;
int eax = sscanf(rdi, "%d%d", rdx, rcx);
if (eax > 1) ...;
else explode_bomb();
```

&emsp;&emsp;`sscanf`函数的具体用法可以问浏览器，总之它和`scanf`的区别是：**scanf是以键盘作为输入源，sscanf是以字符串作为输入源**。这里实际上就是读入两个整数。

&emsp;&emsp;`rsp_8`需要小于等于7，否则蹦蹦炸弹。
```x86asm
 400f6a:	83 7c 24 08 07       	cmpl   $0x7,0x8(%rsp)
  400f6f:	77 3c                	ja     400fad <phase_3+0x6a>

  ......

  400fad:	e8 88 04 00 00       	call   40143a <explode_bomb>
```

&emsp;&emsp;接下来程序会跳转到`*(0x402470 + rsp_8 * 8)`的地方，查看一下内存：
```shell
pwndbg> x/8gx 0x402470
0x402470:	0x0000000000400f7c	0x0000000000400fb9
0x402480:	0x0000000000400f83	0x0000000000400f8a
0x402490:	0x0000000000400f91	0x0000000000400f98
0x4024a0:	0x0000000000400f9f	0x0000000000400fa6
```

&emsp;&emsp;这里面存的是8个地址，所以这里实际上是一个跳转表，实际上仔细对比一下就可以发现这8个地址分别对应`phase_3`内部的8个代码片段：
```x86asm
; section 0:
  400f7c:	b8 cf 00 00 00       	mov    $0xcf,%eax
  400f81:	eb 3b                	jmp    400fbe <phase_3+0x7b>

; section 2:
  400f83:	b8 c3 02 00 00       	mov    $0x2c3,%eax
  400f88:	eb 34                	jmp    400fbe <phase_3+0x7b>

; section 3:
  400f8a:	b8 00 01 00 00       	mov    $0x100,%eax
  400f8f:	eb 2d                	jmp    400fbe <phase_3+0x7b>

; section 4:
  400f91:	b8 85 01 00 00       	mov    $0x185,%eax
  400f96:	eb 26                	jmp    400fbe <phase_3+0x7b>

; section 5:
  400f98:	b8 ce 00 00 00       	mov    $0xce,%eax
  400f9d:	eb 1f                	jmp    400fbe <phase_3+0x7b>

; section 6:
  400f9f:	b8 aa 02 00 00       	mov    $0x2aa,%eax
  400fa4:	eb 18                	jmp    400fbe <phase_3+0x7b>

; section 7:
  400fa6:	b8 47 01 00 00       	mov    $0x147,%eax
  400fab:	eb 11                	jmp    400fbe <phase_3+0x7b>

; section 1:
  400fb9:	b8 37 01 00 00       	mov    $0x137,%eax
```

&emsp;&emsp;他们最终都会跳转到`0x400fbe`，程序会比较`rsp_c`和`%eax`是否相同，不同的话BOOM！！！
```x86asm
 400fbe:	3b 44 24 0c          	cmp    0xc(%rsp),%eax
  400fc2:	74 05                	je     400fc9 <phase_3+0x86>
  400fc4:	e8 71 04 00 00       	call   40143a <explode_bomb>
  400fc9:	48 83 c4 18          	add    $0x18,%rsp
  400fcd:	c3                   	ret    
```

&emsp;&emsp;`rsp_8`是我们输入的第一个数，`rsp_c`是第二个。如果我们第一个数输入0，那么根据跳转表各表项所对应的分支，`%eax`的值最后会是`0xcf`，也就是207。所以这个phase一个可行的解是`0 207`。

# phase 4
&emsp;&emsp;上来和`phase_3`一样做了参数都一模一样的`sscanf`的调用。

&emsp;&emsp;`rsp_8`需要小于等于8，否则炸。

&emsp;&emsp;然后`phase_4`调用`func4(rsp_8, 0, 14)`。函数的返回值必须是0，同时`rsp_c`的值，也就是我们输入的第二个数，也必须为0。
```x86asm
 40103a:	ba 0e 00 00 00       	mov    $0xe,%edx
  40103f:	be 00 00 00 00       	mov    $0x0,%esi
  401044:	8b 7c 24 08          	mov    0x8(%rsp),%edi
  401048:	e8 81 ff ff ff       	call   400fce <func4>         ;func4(rsp_8, 0, 14)
  40104d:	85 c0                	test   %eax,%eax              ;if return value is zero
  40104f:	75 07                	jne    401058 <phase_4+0x4c>
  401051:	83 7c 24 0c 00       	cmpl   $0x0,0xc(%rsp)         ;if rsp_c is zero
  401056:	74 05                	je     40105d <phase_4+0x51>
  401058:	e8 dd 03 00 00       	call   40143a <explode_bomb>
  40105d:	48 83 c4 18          	add    $0x18,%rsp
  401061:	c3                   	ret    
```

&emsp;&emsp;我们开始分析`func4`，我们发现一个很有意思的地方：
```x86asm
 400ff2:	b8 00 00 00 00       	mov    $0x0,%eax
  400ff7:	39 f9                	cmp    %edi,%ecx
  400ff9:	7d 0c                	jge    401007 <func4+0x39>

  ......

  401007:	48 83 c4 08          	add    $0x8,%rsp
  40100b:	c3                   	ret    
```

&emsp;&emsp;这一段代码将返回值设置为0，并且只要`%edi >= %ecx`条件满足就可以退出函数。所以我们希望这个代码段能够被执行。而在这一段代码的上面，正好有一条能够跳转到这个地方的指令：

```x86asm
 400fe2:	39 f9                	cmp    %edi,%ecx
  400fe4:	7e 0c                	jle    400ff2 <func4+0x24>
```

&emsp;&emsp;很好玩的是，这里又要求`%edi <= %ecx`。折腾半天，不就是要求`%edi == %ecx`么。

&emsp;&emsp;最后再看函数`func4`开始部分的汇编代码，我们会发现变参`%rdi`没有被改变，`%ecx`的值也是完全根据后两个常参数0和14（0xe）算出来的。所以在相应位置设置断点，看看`%ecx`的值最后是多少。

```shell
pwndbg> b *0x400fe2
Breakpoint 3 at 0x400fe2
pwndbg> c
Continuing.

Breakpoint 3, 0x0000000000400fe2 in func4 ()
LEGEND: STACK | HEAP | CODE | DATA | RWX | RODATA
─────────────[ REGISTERS / show-flags off / show-compact-regs off ]─────────────
 RAX  0x7
 RBX  0x0
*RCX  0x7
 RDX  0xe
 RDI  0x7
 RSI  0x0
 R8   0x1999999999999999
 R9   0x0
 R10  0x7ffff7dbeac0 (_nl_C_LC_CTYPE_toupper+512) ◂— 0x100000000
 R11  0x7ffff7dbf3c0 (_nl_C_LC_CTYPE_class+256) ◂— 0x2000200020002
 R12  0x7fffffffdfc8 —▸ 0x7fffffffe320 ◂— '/home/dada/Desktop/cmu15213/bomblab/bomb'
 R13  0x400da0 (main) ◂— push rbx
 R14  0x0
 R15  0x7ffff7ffd040 (_rtld_global) —▸ 0x7ffff7ffe2e0 ◂— 0x0
 RBP  0x1
 RSP  0x7fffffffde80 —▸ 0x400da0 (main) ◂— push rbx
*RIP  0x400fe2 (func4+20) ◂— cmp ecx, edi

pwndbg> print $ecx
$1 = 7
```

&emsp;&emsp;`%ecx`的值为7，那么我们输入的第一个数就是7，第二个数为0。
