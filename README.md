# LALR(1) Parser Generator

Require Visual Studio or gcc.
Graphviz is optional.

    $ run null input-expr3-arg.txt
    Compiling: null.c
    conflict 0

           S   E   T      $   +   *   a
     T0    .  s3  s2  |   .   .   .  s1
     T1    .   .   .  |  r4  r4  r4   .
     T2    .   .   .  |  r2  r2  s7   .
     T3    .   .   .  |   o  s4   .   .
     T4    .   .  s6  |   .   .   .  s1
     T5    .   .   .  |   .   .   .   .
     T6    .   .   .  |  r1  r1  s7   .
     T7    .   .   .  |   .   .   .  s8
     T8    .   .   .  |  r3  r3  r3   .
    
    done

    $ dot

[Items](graph.png)
