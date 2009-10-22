#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/ucontext.h>
#include <ruby.h>

static ucontext_t ruby_context;
static ucontext_t main_context;
static char ruby_context_stack[SIGSTKSZ];
static bool ruby_context_finished = false;

static void relay_from_main_to_ruby()
{
    printf("Main: relay_from_main_to_ruby() begin\n");
    swapcontext(&main_context, &ruby_context);
    printf("Main: relay_from_main_to_ruby() end\n");
}

static void relay_from_ruby_to_main()
{
    printf("Ruby: relay_from_ruby_to_main() begin\n");
    swapcontext(&ruby_context, &main_context);
    printf("Ruby: relay_from_ruby_to_main() end\n");
}

static VALUE ruby_context_body_require(char* file)
{
    int status;
    VALUE result = rb_protect((VALUE (*)(VALUE))rb_require, (VALUE)file, &status);

    if (status)
    {
        printf("rb_require('%s') failed with status=%d\n", file, status);

        VALUE exception = rb_gv_get("$!");
        if (RTEST(exception))
        {
            printf("... because an exception was raised:\n");
            fflush(stdout);

            VALUE inspect = rb_inspect(exception);
            rb_io_puts(1, &inspect, rb_stderr);

            VALUE backtrace = rb_funcall(exception, rb_intern("backtrace"), 0);
            rb_io_puts(1, &backtrace, rb_stderr);
        }
    }

    return result;
}

static void ruby_context_body()
{
    printf("Ruby: context begin\n");

    int i;
    for (i = 0; i < 3; i++)
    {
        printf("Ruby: relay %d\n", i);
        relay_from_ruby_to_main();
    }

    printf("Ruby: require 'hello' begin\n");
    ruby_context_body_require("./hello.rb");
    printf("Ruby: require 'hello' end\n");

    printf("Ruby: context end\n");
    ruby_context_finished = true;
    relay_from_ruby_to_main();
}

RUBY_GLOBAL_SETUP

int main(int argc, char** argv)
{
    ruby_sysinit(&argc, &argv);
    {
        RUBY_INIT_STACK;
        ruby_init();
        ruby_init_loadpath();

        /* initialize Ruby context */
        ruby_context.uc_link          = &main_context;
        ruby_context.uc_stack.ss_sp   = ruby_context_stack;
        ruby_context.uc_stack.ss_size = sizeof(ruby_context_stack);
        getcontext(&ruby_context);
        makecontext(&ruby_context, (void (*)(void)) ruby_context_body, 0);

        /* relay control to Ruby until it is finished */
        ruby_context_finished = false;
        while (!ruby_context_finished)
        {
            relay_from_main_to_ruby();
        }

        printf("Main: Goodbye!\n");
    }

    return ruby_cleanup(0);
}
