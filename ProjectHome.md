This garbage collector uses a mark-sweep algorithm . Unlike [BDW conservative garbage collector](http://www.hpl.hp.com/personal/Hans_Boehm/gc/) , it doesn't guess the pointer . So you should link the objects (memory block) manually by call **gc\_link** . Namely , you should tell the collector that a memory block's life time depends on the correlative one's .

On the other hand, you should give the scope of temporary variable on the stack manually.
It will be easy if you call **gc\_enter** at the first line of the function, and call **gc\_leave** at the last line. btw, you don't need to call **gc\_enter** / **gc\_leave** every time. For example, you can only call **gc\_enter** before your main loop and call **gc\_leave** after the main loop returned. (It may waste some memory, but it works.)

If you have done those correctly , **gc\_collect** will be able to determine which memory block won't be accessed any longer, and then free them.

ps. [Another description here](http://blog.codingnow.com/2008/06/gc_for_c.html) , if you can read Chinese.