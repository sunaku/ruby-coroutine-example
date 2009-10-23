#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef HAVE_SYS_UCONTEXT_H
#include <sys/ucontext.h>
#endif

#ifdef HAVE_CONTEXT_H
#include <ucontext.h>
#endif

#include <ruby.h>

static ucontext_t ruby_context;
static ucontext_t main_context;
static char ruby_context_stack[SIGSTKSZ];
static bool ruby_context_finished;

static void relay_from_main_to_ruby()
{
    printf("Relay: main => ruby\n");
    swapcontext(&main_context, &ruby_context);
    printf("Relay: main <= ruby\n");
}

static VALUE relay_from_ruby_to_main(VALUE self)
{
    printf("Relay: ruby => main\n");
    swapcontext(&ruby_context, &main_context);
    printf("Relay: ruby <= main\n");
    return Qnil;
}

static VALUE ruby_context_body_require(char* file)
{
    int error;
    VALUE result = rb_protect((VALUE (*)(VALUE))rb_require,
                              (VALUE)file, &error);

    if (error)
    {
        printf("rb_require('%s') failed with status=%d\n",
               file, error);

        VALUE exception = rb_gv_get("$!");
        if (RTEST(exception))
        {
            printf("... because an exception was raised:\n");
            fflush(stdout);

            VALUE inspect = rb_inspect(exception);
            rb_io_puts(1, &inspect, rb_stderr);

            VALUE backtrace = rb_funcall(
                exception, rb_intern("backtrace"), 0);
            rb_io_puts(1, &backtrace, rb_stderr);
        }
    }

    return result;
}

static void ruby_context_body()
{
    printf("Context: begin\n");

    int i;
    for (i = 0; i < 2; i++)
    {
        printf("Context: relay %d\n", i);
        relay_from_ruby_to_main(Qnil);
    }

    printf("Context: Ruby begin\n");

    #ifdef HAVE_RUBY_SYSINIT
    int argc = 0;
    char** argv = {""};
    ruby_sysinit(&argc, &argv);
    #endif
    {
        RUBY_INIT_STACK;
        ruby_init();
        ruby_init_loadpath();

        /* allow Ruby script to relay */
        rb_define_module_function(rb_mKernel, "relay_from_ruby_to_main",
                                  relay_from_ruby_to_main, 0);

        /* run the "hello world" Ruby script */
        printf("Ruby: require 'hello' begin\n");
        ruby_context_body_require("./hello.rb");
        printf("Ruby: require 'hello' end\n");

        ruby_cleanup(0);
    }

    printf("Context: Ruby end\n");

    printf("Context: end\n");

    ruby_context_finished = true;
    relay_from_ruby_to_main(Qnil);
}

#ifdef RUBY_GLOBAL_SETUP
RUBY_GLOBAL_SETUP
#endif

main()
{
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
