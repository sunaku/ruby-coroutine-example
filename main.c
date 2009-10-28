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

static ucontext_t main_context;
static ucontext_t ruby_context;
static size_t ruby_context_stack_size;
static char ruby_context_stack[32*1024]; // 32 KiB is enough for 1.9
static bool ruby_context_finished;

static size_t detect_stack_watermark()
{
    char* stack_end = ruby_context_stack;
    char* stack_start = stack_end + ruby_context_stack_size;

    while (stack_end <= stack_start)
    {
        if (*stack_end != 0)
        {
            return stack_start - stack_end;
        }

        stack_end++;
    }

    return 0;
}

static void relay_from_main_to_ruby()
{
    char dummy;
    printf("Relay: main => ruby   stack=%p\n", &dummy); fflush(stdout);
    swapcontext(&main_context, &ruby_context);
    printf("Relay: main <= ruby\n"); fflush(stdout);
}

static VALUE relay_from_ruby_to_main(VALUE self)
{
    char dummy;
    printf("Relay: ruby => main   stack=%p\n", &dummy); fflush(stdout);
    swapcontext(&ruby_context, &main_context);
    printf("Relay: ruby <= main\n"); fflush(stdout);
    return Qnil;
}

static VALUE ruby_context_body_require(const char* file)
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

    printf("Context: stack used = %lu\n", detect_stack_watermark());

    #ifdef HAVE_RUBY_SYSINIT
    int argc = 4;
    char** argv = malloc(sizeof(char*) * argc);
    argv[0] = "-v";
    argv[1] = "-w";
    argv[2] = "hello.rb";
    argv[3] = "whuzza";
    ruby_sysinit(&argc, &argv);
    printf("Context: after ruby_sysinit(), stack used = %lu\n", detect_stack_watermark());
    #endif
    {
        #ifdef HAVE_RUBY_BIND_STACK
        ruby_bind_stack(
            (VALUE*)(ruby_context_stack),                          /* lower */
            (VALUE*)(ruby_context_stack + ruby_context_stack_size) /* upper */
        );
        #endif

        RUBY_INIT_STACK;
        printf("Context: RUBY_INIT_STACK stack used = %lu\n", detect_stack_watermark());
        ruby_init();
        printf("Context: ruby_init() stack used = %lu\n", detect_stack_watermark());
        ruby_init_loadpath();
        printf("Context: ruby_init_loadpath() stack used = %lu\n", detect_stack_watermark());

        /* allow Ruby script to relay */
        rb_define_module_function(rb_mKernel, "relay_from_ruby_to_main",
                                  relay_from_ruby_to_main, 0);
        printf("Context: rb_define_module_function() stack used = %lu\n", detect_stack_watermark());

        /* run the "hello world" Ruby script */
        printf("Ruby: require 'hello' begin\n");
        ruby_context_body_require("./hello.rb");
        printf("Ruby: require 'hello' end\n");

        // ruby_run_node(ruby_options(argc, argv));
        printf("Context: ruby_run_node() stack used = %lu\n", detect_stack_watermark());
        ruby_cleanup(0);

        printf("Context: ruby_cleanup() stack used = %lu\n", detect_stack_watermark());
    }

    printf("Context: Ruby end\n");

    printf("Context: end\n");

    ruby_context_finished = true;
    relay_from_ruby_to_main(Qnil);
}

#ifdef RUBY_GLOBAL_SETUP
RUBY_GLOBAL_SETUP
#endif

int main()
{
    /* create System V context to house Ruby */
    ruby_context_stack_size = sizeof(ruby_context_stack);
    memset(ruby_context_stack, 0, ruby_context_stack_size);

    ruby_context.uc_link          = &main_context;
    ruby_context.uc_stack.ss_sp   = ruby_context_stack;
    ruby_context.uc_stack.ss_size = ruby_context_stack_size;
    getcontext(&ruby_context);
    makecontext(&ruby_context, (void (*)(void)) ruby_context_body, 0);

    /* relay control to Ruby until it is finished */
    ruby_context_finished = false;
    while (!ruby_context_finished)
    {
        relay_from_main_to_ruby();
    }

    printf("Main: Goodbye!\n");
    return 0;
}
