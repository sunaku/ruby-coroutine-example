#include <stdio.h>
#include <ruby.h>

VALUE
my_fiber_body(VALUE arg)
{
    printf("Fiber: Entering fiber body\n");

    rb_require("./hello.rb");

    printf("Fiber: Exiting fiber body\n");

    return Qtrue;
}

VALUE
create_my_fiber(VALUE arg)
{
    VALUE fib = rb_fiber_new(my_fiber_body, arg);

    printf("create_my_fiber: Created my fiber=");
    fflush(stdout);

    VALUE dump = rb_inspect(fib);
    rb_io_puts(1, &dump, rb_stdout);

    return fib;
}

VALUE
resume_my_fiber(VALUE fib)
{
    printf("resume_my_fiber: Going to resume fiber=");
    fflush(stdout);

    VALUE dump = rb_inspect(fib);
    rb_io_puts(1, &dump, rb_stdout);

    if (RTEST(rb_fiber_alive_p(fib)))
    {
        VALUE result = rb_fiber_resume(fib, 0, 0);

        printf("resume_my_fiber: Fiber yielded value=");
        fflush(stdout);

        dump = rb_inspect(result);
        rb_io_puts(1, &dump, rb_stdout);

        return result;
    }
    else
    {
        printf("resume_my_fiber: Fiber is dead! cannot resume\n");
        return Qfalse;
    }
}

RUBY_GLOBAL_SETUP

int
main(int argc, char** argv)
{
    ruby_sysinit(&argc, &argv);
    {
        RUBY_INIT_STACK;
        ruby_init();

        VALUE fib = rb_protect(create_my_fiber, Qnil, 0);

        printf("Main: Outside rb_protect()\n");
        /*
            NOTE: If I resume the fiber out here, then a segfault occurs.
                  Is this because rb_protect() provides a running thread?
        */

        printf("Main: Going to resume fiber many times...\n");

        VALUE count;
        do
        {
            count = rb_protect(resume_my_fiber, fib, 0);
        }
        while (RTEST(count));

        VALUE err = rb_gv_get("$!");
        if (RTEST(err))
        {
            printf("Main: An exception was raised:\n");
            fflush(stdout);

            VALUE dump = rb_inspect(err);
            rb_io_puts(1, &dump, rb_stderr);

            VALUE trace = rb_funcall(err, rb_intern("backtrace"), 0);
            rb_io_puts(1, &trace, rb_stderr);
        }

        printf("Main: Goodbye!\n");
    }

    return ruby_cleanup(0);
}
