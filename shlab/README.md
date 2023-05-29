&emsp;&emsp;CS:APP Lab网站：<https://csapp.cs.cmu.edu/3e/labs.html>

&emsp;&emsp;点击`Self-Study Handout`和`writeup`下载到本地文件夹，使用命令`tar xvf shlab-handout.tar`将文件解压缩得到一个同名文件夹。文件夹里有很多的文件，参阅`writeup`，我们需要做的是修改`tsh.c`中的若干个函数：

- `eval`：解析并执行来自终端的命令。
- `builtin_cmd`：解析并执行内部命令。
- `do_fgbg`：执行`fg`和`bg`这两个内部命令。
- `waitfg`：等待一个前台作业（进程）的终止或者停止。
- `sigchld_handler`：SIGCHLD信号处理程序。
- `sigint_handler`：SIGINT信号处理程序，这个信号会终止当前前台作业。
- `sigtstp_handler`：SIGTSTP信号处理程序，这个信号会暂停当前前台作业。

&emsp;&emsp;在此之前，为了使文件的组织更好看，我将16个trace文件并入了同级的traces目录下，并对`makefile`进行了修改。

&emsp;&emsp;tsh需要实现的功能参见writeup的General Overview of Unix Shells部分。主要实现以下几点：
- 能够执行外部命令
- 内部命令`jobs`在终端打印当前所有作业（进程）的信息，包括进程id、状态和对应的命令。
- 内部指令`fg <job>`能够将指定的后台正在运行的或者停止的作业挂到前台运行。
- 内部命令`bg <job>`能够将指定的后台停止状态的进程重新启动并在后台继续运行。
- 内部命令`quit`能够退出shell。

&emsp;&emsp;下面根据16个trace文件展示的样例的顺序，介绍一下各个函数的实现，顺带着会将各个功能的实现。

# eval
&emsp;&emsp;就是照着书上源码抄的。根据writeup上的Hints做了一些修改：
1. 父进程在fork子进程前需要屏蔽`SIGCHLD`信号，否则会产生书上8.5.6节产生的竞争条件，即在addjob之前进行了deletejob的操作导致程序语义出错。
2. 进行addjob和deletejob操作前需要屏蔽所有信号，操作完成后解除屏蔽。
3. 由于继承了父进程的屏蔽信号，子进程开始前需解除对SIGCHLD信号的屏蔽。
4. 由于在终端键入Ctrl-C或Ctrl-Z默认会将一个终止信号SIGINT或一个停止信号SIGTSTP送给前台进程组中所有的进程，包括你的shell。所以在子进程调用`execve`之前需要调用`setpgid(0, 0)`，将子进程挂到一个组id和子进程id一样的新的进程组里。当shell收到来自终端的信号，会将该信号传递至应当接收这个信号的进程组那里。

&emsp;&emsp;源代码如下：
```c
/*
 * eval - Evaluate the command line that the user has just typed in
 *
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.
 */
void eval(char *cmdline)
{
    char *argv[MAXARGS];
    pid_t pid;
    int bg;

    bg = parseline(cmdline, argv);

    /* Block SIGCHLD before fork a child */
    sigset_t s1, s2, s3; // warning of sigset_t being used but undefined if without declaration
    sigset_t *mask_all = &s1, *mask_sigchild = &s2, *mask_prev = &s3;
    sigfillset(mask_all);
    sigemptyset(mask_sigchild);
    sigaddset(mask_sigchild, SIGCHLD);

    if (!builtin_cmd(argv))
    {
        sigprocmask(SIG_BLOCK, mask_sigchild, mask_prev);
        if ((pid = fork()) == 0)
        {
            sigprocmask(SIG_SETMASK, mask_prev, NULL);  // unblock SIGCHLD
            setpgid(0, 0);  // put child process in a new proces group
            if (execve(argv[0], argv, environ) < 0)
            {
                printf("%s: Command not found.\n", argv[0]);
                exit(0);
            }
        }

        /* Block all the signals when adding a job */
        sigprocmask(SIG_BLOCK, mask_all, NULL);
        addjob(jobs, pid, bg ? BG : FG, cmdline);
        sigprocmask(SIG_SETMASK, mask_prev, NULL);

        if (!bg)
        {
            waitfg(pid);
        }
        else
        {
            printf("[%d] (%d) %s", nextjid - 1, pid, cmdline);
            fflush(NULL);
        }
    }

    return;
}
```

# builtin_cmd
&emsp;&emsp;内置命令最难搞的当属`fg`和`bg`，然而这两个命令是在`do_fgbg`函数中实现的，所以这个函数写下来没啥含金量。
```c
/*
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.
 */
int builtin_cmd(char **argv)
{
    char *s = argv[0];

    if (!strcmp(s, "quit"))
        exit(0);
    if (!strcmp(s, "&"))
        return 1;

    if (!strcmp(s, "jobs"))
    {
        listjobs(jobs);
        return 1;
    }

    if (!strcmp(s, "fg") || !strcmp(s, "bg"))
    {
        do_bgfg(argv);
        return 1;
    }

    return 0; /* not a builtin command */
}
```

# waitfg
&emsp;&emsp;writeup中推荐的一个实现方式是在`waitfg`当中进行忙等待，对`waitpid`的调用放到`SIGCHLD`信号处理程序中，这样的实现更加简洁。

&emsp;&emsp;我在这个写函数时踩了个坑，一开始while循环只判定进程有没有终止，导致后面测试Ctrl+Z时喜提死循环。在这里while循环中，终止和停止两个状态都要判断。
```c
void waitfg(pid_t pid)
{
    /* while process pid is not terminated OR STOPPED */
    /* dead loop if ignore the circumstance of the process being stopped!!! */
    while (getjobpid(jobs, pid) != NULL && getjobpid(jobs, pid)->state != ST)
        sleep(1);
    return;
}
```

# sigchid_handler
&emsp;&emsp;writeup要求对`waitpid`的调用不能阻塞，所以调用时第三个参数`option`需设置为`WNOHANG`以取消忙等待。

&emsp;&emsp;调用`deletejob`之前也需要堵塞所有的信号，和`addjob`一样。
```c
/*
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.
 */
void sigchld_handler(int sig)
{
    sigset_t s1, s2;    
    sigset_t *mask_all = &s1, *mask_prev = &s2;
    int old_errno = errno;

    sigfillset(mask_all);

    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
    {
        sigprocmask(SIG_BLOCK, mask_all, mask_prev);
        deletejob(jobs, pid);
        sigprocmask(SIG_SETMASK, mask_prev, NULL);
    }

    if (pid == -1 && errno != ECHILD)
        unix_error("waitfg: waitpid error");

    errno = old_errno;
    return;
}
```

# sigint_handler & sigtstp_handler
&emsp;&emsp;来自终端的Ctrl-C和Ctrl-Z会被shell接收。由于前台作业最多有一个，所以直接在作业列表里找到前台进程并用kill函数将信号发送至前台作业所在进程组即可，并打印信息。
```c
/*
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.
 */
void sigint_handler(int sig)
{
    sigset_t s1, s2;
    sigset_t* mask_all = &s1, *mask_prev = &s2;
    sigfillset(mask_all);

    sigprocmask(SIG_BLOCK, mask_all, mask_prev);

    pid_t pid = fgpid(jobs);
    if (pid)
    {
        kill(-pid, SIGINT);
        printf("Job [%d] (%d) terminated by signal 2\n", pid2jid(pid), pid);
    }

    sigprocmask(SIG_SETMASK, mask_prev, NULL);

    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.
 */
void sigtstp_handler(int sig)
{
    sigset_t s1, s2;
    sigset_t* mask_all = &s1, *mask_prev = &s2;
    sigfillset(mask_all);

    sigprocmask(SIG_BLOCK, mask_all, mask_prev);

    struct job_t *job = NULL;
    pid_t pid = fgpid(jobs);
    if (pid)
    {
        job = getjobpid(jobs, pid);
        job->state = ST;
        kill(-pid, SIGTSTP);
        printf("Job [%d] (%d) stopped by signal 20\n", job->jid, pid);
    }

    sigprocmask(SIG_SETMASK, mask_prev, NULL);

    return;
}
```

# do_fgbg
- fg命令：在作业列表中改变指定作业（进程）的状态为`FG`，发送`SIGCONT`命令，最后进入waitfg函数等待这个前台进程的结束。注意调用waitfg函数之前要恢复接收所有的信号，不然进程会无法接收SIGCHLD指令而死循环。
- bg命令：不需要`waitfg`，其他同上
  
&emsp;&emsp;对fg和bg的错误处理略，具体参考trace14提高的样例。
```c
/*
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv)
{
    char*s = argv[0], *s0 = argv[1];
    struct job_t* job = NULL;
    int pid, id;

    sigset_t s1, s2;
    sigset_t* mask_all = &s1, *mask_prev = &s2;
    sigfillset(mask_all);

    if (s0 == NULL)
    {
        printf("%s command requires PID or %%jobid argument\n", s);
        return;
    }

    if (*s0 == '%')
    {
        for (int i = 1; s0[i] != '\0'; i++)
        {
            if (s0[i] < '0' || s0[i] > '9')
            {
                printf("%s: argument must be a PID or %%jobid\n", s);
                return;
            }
        }

        id = atoi(s0 + 1);
        job = getjobjid(jobs, id);

        if (job == NULL)
        {
            printf("%%%d: No such job\n", id);
            return;
        }
    }
    else
    {
        for (int i = 0; s0[i] != '\0'; i++)
        {
            if (s0[i] < '0' || s0[i] > '9')
            {
                printf("%s: argument must be a PID or %%jobid\n", s);
                return;
            }
        }

        id = atoi(s0);
        job = getjobpid(jobs, id);

        if (job == NULL)
        {
            printf("(%d): No such process\n", id);
            return;
        }
    }

    if (!strcmp(s, "fg"))
    {
        sigprocmask(SIG_BLOCK, mask_all, mask_prev);

        pid = job->pid;
        int oldstate = job->state;
        job->state = FG;
        if (oldstate == ST) kill(-pid, SIGCONT);

        sigprocmask(SIG_SETMASK, mask_prev, NULL);

        waitfg(pid); // be aware of the code order
    }
    else
    {
        sigprocmask(SIG_BLOCK, mask_all, mask_prev);

        pid = job->pid;
        int oldstate = job->state;
        job->state = BG;
        if (oldstate == ST) kill(-pid, SIGCONT);

        printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline);

        sigprocmask(SIG_SETMASK, mask_prev, NULL);
    }

    return;
}

```

# 改进
&emsp;&emsp;到最后一个样例出问题了。

&emsp;&emsp;最后一个样例是程序自己给自己发送一个终止或者停止信号，不像终端Ctrl-C&Z，这种情况下shell是无法捕获这些信号的，自然也就无法给对应的进程发送信号。

&emsp;&emsp;但是有一个信号shell是可以收到的，那就是`SIGCHLD`。

&emsp;&emsp;waitpid函数提供了很多宏参数来规定其行为，仅仅设置`WNOHANG`返回的也只是被终止的进程id，而处于停止状态的进程我们忽略了，然而设置`WNOHANG | WUNTRACED`就可以同时捕获处于终止或停止状态的进程id。

&emsp;&emsp;同时还有很多宏函数根据status值来检查返回进程的状态，接下来会用到这几个：
- `WIFSIGNALED(status)`：如果子进程因为一个信号终止，就返回真。
- `WIFSTOPPED(status)`：如果引起返回的子进程是停止状态，就返回真。

&emsp;&emsp;首先检查返回的进程是不是处于终止状态，如果是，改变进程状态并输出信息，同时直接退出循环，否则waitpid会陷入死循环。我这里做了个假设，就是`waitpid`永远先返回的是终止状态的进程，返回的如果是停止状态的进程说明其他终止状态的进程已经回收了，或者说没有。

```c
job = getjobpid(jobs, pid);
jid = job->jid;

if (WIFSTOPPED(status))
{
    sigprocmask(SIG_BLOCK, mask_all, mask_prev);
    job->state = ST;
    printf("Job [%d] (%d) stopped by signal 20\n", jid, pid);
    sigprocmask(SIG_SETMASK, mask_prev, NULL);
    break;
}
```

&emsp;&emsp;否则程序就是处于终止状态。进程终止有两种情况：
1. 寿终正寝，正常终止，此时`WIFEXITED(status)`为真。
2. 非正常死亡，被一个信号所终止，此时`WIFSIGNALED(status)`为真。

&emsp;&emsp;如果程序是正常退出的话，除了deletejob并不需要额外做什么，然而被信号终止的情况下需要额外输出一条进程终止信息。

```c
sigprocmask(SIG_BLOCK, mask_all, mask_prev);
deletejob(jobs, pid);
if (WIFSIGNALED(status))
    printf("Job [%d] (%d) terminated by signal 2\n", jid, pid);
sigprocmask(SIG_SETMASK, mask_prev, NULL);
```

&emsp;&emsp;改进后的`SIGCHLD`信号处理程序如下：
```c
/*
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.
 */
void sigchld_handler(int sig)
{
    sigset_t s1, s2;
    sigset_t *mask_all = &s1, *mask_prev = &s2;
    struct job_t *job = NULL; 
    int old_errno = errno, pid, jid, status; //stop_iter = 0;

    sigfillset(mask_all);

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0)
    {
        job = getjobpid(jobs, pid);
        jid = job->jid;

        if (WIFSTOPPED(status))
        {
            sigprocmask(SIG_BLOCK, mask_all, mask_prev);
            job->state = ST;
            printf("Job [%d] (%d) stopped by signal 20\n", jid, pid);
            sigprocmask(SIG_SETMASK, mask_prev, NULL);
            break;
        }

        sigprocmask(SIG_BLOCK, mask_all, mask_prev);
        deletejob(jobs, pid);
        if (WIFSIGNALED(status))
            printf("Job [%d] (%d) terminated by signal 2\n", jid, pid);
        sigprocmask(SIG_SETMASK, mask_prev, NULL);
    }

    if (pid == -1 && errno != ECHILD)
        unix_error("waitfg: waitpid error");

    errno = old_errno;
    return;
}
```

&emsp;&emsp;这个情况下，原来很多`SIGINT`和`SIGTSTP`信号处理程序做的事被`SIGCHLD`信号处理程序给做了。它们现在只需要在shell被输入一个Ctrl-C&Z之后，往相应的前台进程发送一个信号就可以。
```c
/*
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.
 */
void sigint_handler(int sig)
{
    sigset_t s1, s2;
    sigset_t* mask_all = &s1, *mask_prev = &s2;
    sigfillset(mask_all);

    sigprocmask(SIG_BLOCK, mask_all, mask_prev);
    pid_t pid = fgpid(jobs);
    if (pid) kill(-pid, SIGINT);
    sigprocmask(SIG_SETMASK, mask_prev, NULL);

    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.
 */
void sigtstp_handler(int sig)
{
    sigset_t s1, s2;
    sigset_t* mask_all = &s1, *mask_prev = &s2;
    sigfillset(mask_all);

    sigprocmask(SIG_BLOCK, mask_all, mask_prev);
    pid_t pid = fgpid(jobs);
    if (pid) kill(-pid, SIGTSTP);
    sigprocmask(SIG_SETMASK, mask_prev, NULL);

    return;
}
```
