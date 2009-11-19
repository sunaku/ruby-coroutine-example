#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <ruby.h>

static pthread_t ruby_context;
static pthread_mutex_t main_context_lock;
static pthread_mutex_t ruby_context_lock;

static size_t ruby_context_stack_size;
static char ruby_context_stack[4*(1024*1024)]; // 4 MiB
static bool ruby_context_finished;

static void relay_from_main_to_ruby()
{
    printf("Relay: main => ruby\n");
    pthread_mutex_unlock(&ruby_context_lock);
    pthread_mutex_lock(&main_context_lock);
    printf("Relay: main <= ruby\n");
}

static VALUE relay_from_ruby_to_main(VALUE self)
{
    printf("Relay: ruby => main\n");
    pthread_mutex_unlock(&main_context_lock);
    pthread_mutex_lock(&ruby_context_lock);
    printf("Relay: ruby <= main\n");
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

static void ruby_context_body(void* dummy)
{
    printf("Context: waiting for initial asynchronous relay from main\n");

    relay_from_ruby_to_main(Qnil);

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
        #ifdef HAVE_RUBY_BIND_STACK
        ruby_bind_stack(
            (VALUE*)(ruby_context_stack),                          /* lower */
            (VALUE*)(ruby_context_stack + ruby_context_stack_size) /* upper */
        );
        #endif

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

    pthread_exit(NULL);
}

#ifdef RUBY_GLOBAL_SETUP
RUBY_GLOBAL_SETUP
#endif

int main()
{
    /* initialize the relay mechanism */
    pthread_mutex_init(&ruby_context_lock, NULL);
    pthread_mutex_lock(&ruby_context_lock);
    pthread_mutex_init(&main_context_lock, NULL);
    pthread_mutex_lock(&main_context_lock);

    /* create pthread to house Ruby */
    ruby_context_stack_size = sizeof(ruby_context_stack);

    pthread_attr_t attr;
    pthread_attr_init(&attr);

    #ifdef HAVE_PTHREAD_ATTR_SETSTACK
        pthread_attr_setstack(&attr, ruby_context_stack, ruby_context_stack_size);
    #else
        pthread_attr_setstackaddr(&attr, ruby_context_stack);
        pthread_attr_setstacksize(&attr, ruby_context_stack_size);
    #endif

    int error = pthread_create(&ruby_context, &attr, &ruby_context_body, NULL);
    if (error) {
        printf("ERROR: pthread_create() returned %d\n", error);
        return 1;
    }

    /* relay control to Ruby until it is finished */
    ruby_context_finished = false;
    while (!ruby_context_finished)
    {
        relay_from_main_to_ruby();
    }

    printf("Main: Goodbye!\n");
    return 0;
}
