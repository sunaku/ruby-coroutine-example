/* stack size of the couroutine that will run Ruby */
#define DEMONSTRATION_STACK_SIZE (4*(1024*1024)) /* 4 MiB */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ruby.h>

#ifdef DEMONSTRATE_PCL
#include <pcl.h>
static coroutine_t ruby_coroutine;
#endif

#ifdef DEMONSTRATE_PTHREAD
#include <pthread.h>
static pthread_t ruby_coroutine;
static pthread_mutex_t main_coroutine_lock;
static pthread_mutex_t ruby_coroutine_lock;
#endif

#ifdef DEMONSTRATE_UCONTEXT
#ifdef HAVE_SYS_UCONTEXT_H
#include <sys/ucontext.h>
#else
#include <ucontext.h>
#endif
static ucontext_t main_coroutine;
static ucontext_t ruby_coroutine;
#endif

#ifdef DEMONSTRATE_STATIC
static size_t ruby_coroutine_stack_size;
static char ruby_coroutine_stack[DEMONSTRATION_STACK_SIZE];
#endif

#ifdef DEMONSTRATE_DYNAMIC
static size_t ruby_coroutine_stack_size = DEMONSTRATION_STACK_SIZE;
static char* ruby_coroutine_stack;
#endif

static bool ruby_coroutine_finished;

/* puts the Ruby coroutine in control */
static void relay_from_main_to_ruby()
{
    printf("Relay: main => ruby\n");

#ifdef DEMONSTRATE_PCL
    co_call(ruby_coroutine);
#endif

#ifdef DEMONSTRATE_PTHREAD
    pthread_mutex_unlock(&ruby_coroutine_lock);
    pthread_mutex_lock(&main_coroutine_lock);
#endif

#ifdef DEMONSTRATE_UCONTEXT
    swapcontext(&main_coroutine, &ruby_coroutine);
#endif

    printf("Relay: main <= ruby\n");
}

/* puts the main C program in control */
static VALUE relay_from_ruby_to_main(VALUE self)
{
    printf("Relay: ruby => main\n");

#ifdef DEMONSTRATE_PCL
    co_resume();
#endif

#ifdef DEMONSTRATE_PTHREAD
    pthread_mutex_unlock(&main_coroutine_lock);
    pthread_mutex_lock(&ruby_coroutine_lock);
#endif

#ifdef DEMONSTRATE_UCONTEXT
    swapcontext(&ruby_coroutine, &main_coroutine);
#endif

    printf("Relay: ruby <= main\n");
    return Qnil;
}

static VALUE ruby_coroutine_body_require(const char* file)
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

static void ruby_coroutine_body(
#ifdef DEMONSTRATE_PTHREAD
    void* dummy_argument_that_is_not_used
#endif
)
{
#ifdef DEMONSTRATE_PTHREAD
    printf("Coroutine: waiting for initial asynchronous relay from main\n");
    relay_from_ruby_to_main(Qnil);
#endif

    printf("Coroutine: begin\n");

    int i;
    for (i = 0; i < 2; i++)
    {
        printf("Coroutine: relay %d\n", i);
        relay_from_ruby_to_main(Qnil);
    }

    printf("Coroutine: Ruby begin\n");

#ifdef HAVE_RUBY_SYSINIT
    int argc = 0;
    char** argv = {""};
    ruby_sysinit(&argc, &argv);
#endif
    {
#ifdef HAVE_RUBY_BIND_STACK
        ruby_bind_stack(
                /* lower memory address */
                (VALUE*)(ruby_coroutine_stack),

                /* upper memory address */
                (VALUE*)(ruby_coroutine_stack + ruby_coroutine_stack_size)
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
        ruby_coroutine_body_require("./hello.rb");
        printf("Ruby: require 'hello' end\n");

        ruby_cleanup(0);
    }

    printf("Coroutine: Ruby end\n");

    printf("Coroutine: end\n");

    ruby_coroutine_finished = true;
    relay_from_ruby_to_main(Qnil);

#ifdef DEMONSTRATE_PTHREAD
    pthread_exit(NULL);
#endif
}

#ifdef RUBY_GLOBAL_SETUP
RUBY_GLOBAL_SETUP
#endif

int main()
{
#ifdef DEMONSTRATE_STATIC
    ruby_coroutine_stack_size = sizeof(ruby_coroutine_stack);
#endif

#ifdef DEMONSTRATE_DYNAMIC
    /* allocate the coroutine stack */
    ruby_coroutine_stack = malloc(ruby_coroutine_stack_size);
    if (!ruby_coroutine_stack)
    {
        fprintf(stderr, "Could not allocate %lu bytes!\n", ruby_coroutine_stack_size);
        return 1;
    }
#endif

#ifdef DEMONSTRATE_PCL
    /* create coroutine to house Ruby */
    ruby_coroutine = co_create(ruby_coroutine_body, NULL,
            ruby_coroutine_stack, ruby_coroutine_stack_size);
#endif

#ifdef DEMONSTRATE_PTHREAD
    /* initialize the relay mechanism */
    pthread_mutex_init(&ruby_coroutine_lock, NULL);
    pthread_mutex_lock(&ruby_coroutine_lock);
    pthread_mutex_init(&main_coroutine_lock, NULL);
    pthread_mutex_lock(&main_coroutine_lock);

    /* create pthread to house Ruby */
    pthread_attr_t attr;
    pthread_attr_init(&attr);

#ifdef HAVE_PTHREAD_ATTR_SETSTACK
    pthread_attr_setstack(&attr, ruby_coroutine_stack, ruby_coroutine_stack_size);
#else
    pthread_attr_setstackaddr(&attr, ruby_coroutine_stack);
    pthread_attr_setstacksize(&attr, ruby_coroutine_stack_size);
#endif

    int error = pthread_create(&ruby_coroutine, &attr, &ruby_coroutine_body, NULL);
    if (error) {
        printf("ERROR: pthread_create() returned %d\n", error);
        return 1;
    }
#endif

#ifdef DEMONSTRATE_UCONTEXT
    /* create System V context to house Ruby */
    ruby_coroutine.uc_link          = &main_coroutine;
    ruby_coroutine.uc_stack.ss_sp   = ruby_coroutine_stack;
    ruby_coroutine.uc_stack.ss_size = ruby_coroutine_stack_size;
    getcontext(&ruby_coroutine);
    makecontext(&ruby_coroutine, (void (*)(void)) ruby_coroutine_body, 0);
#endif

    /* relay control to Ruby until it is finished */
    ruby_coroutine_finished = false;
    while (!ruby_coroutine_finished)
    {
        relay_from_main_to_ruby();
    }

    printf("Main: Goodbye!\n");
    return 0;
}
