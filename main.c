#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/ucontext.h>
#include <ruby.h>

static ucontext_t ruby_context;
static ucontext_t main_context;
static char ruby_context_stack[SIGSTKSZ];
static bool ruby_context_finished;

static void relay_from_main_to_ruby()
{
    printf("Main: relay_from_main_to_ruby() begin\n");
    swapcontext(&main_context, &ruby_context);
    printf("Main: relay_from_main_to_ruby() end\n");
}

static VALUE relay_from_ruby_to_main(VALUE self)
{
    printf("Ruby: relay_from_ruby_to_main() begin\n");
    swapcontext(&ruby_context, &main_context);
    printf("Ruby: relay_from_ruby_to_main() end\n");
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
    printf("Ruby: context begin\n");

    int i;
    for (i = 0; i < 3; i++)
    {
        printf("Ruby: relay %d\n", i);
        relay_from_ruby_to_main(Qnil);
    }


    /* run the "hello world" Ruby script */
    printf("Ruby: require 'hello' begin\n");
    ruby_context_body_require("./hello.rb");
    printf("Ruby: require 'hello' end\n");


    printf("Ruby: context end\n");

    ruby_context_finished = true;
    relay_from_ruby_to_main(Qnil);
}

#ifdef RUBY_GLOBAL_SETUP
RUBY_GLOBAL_SETUP
#endif

int main(int argc, char** argv)
{
    #ifdef HAVE_RUBY_SYSINIT
    ruby_sysinit(&argc, &argv);
    #endif
    {
        RUBY_INIT_STACK;
        ruby_init();
        ruby_init_loadpath();

        /* allow Ruby script to relay */
        rb_define_module_function(rb_mKernel, "relay_from_ruby_to_main",
                                  relay_from_ruby_to_main, 0);

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
