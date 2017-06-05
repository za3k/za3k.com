[Pixel Art Tutorials](tutorials.md) >> [Mark's Pixel Art Tutorial](mark.md) >> Displaying Pixel Art

At some point in time you may want to save your art into an external file for distribution or display on a web-page.
Some file formats are suitable, others are not.

---

#What not to use :

*.BMP* - 16 million possible colours for each pixel. Extremely inefficient for pixel-art storage.
*.JPG* - Lossy compression will smudge your nice crisp art in a variety of horrible ways.

---

#What you should use :

*.GIF* - Saves images with up to 256 different colours. Small filesize allows fast loading. Loss of image quality only occurs when you excede 256 colours, though it is unlikely you will reach this point unless you are saving a composite of many sprites.
*.PNG* - Similiar to gif, except that it will allow you to save images with more than 256 colours without image loss. Technically better, but not well supported by Internet Explorer (boo, hiss).

---

By far the most commonly used format for pixel art is *.GIF*. As well as having been well established, it also supports transparency (usually RGB value 0,0,0) and animation. This makes it excedingly useful for displaying game sprites.

---

#Programs which can save .GIF files :

Unfortunately it can be difficult to find a free program which saves .gif format files well. Until recently the format was patented and distributors would have to pay a fee to support it. I recommend [Painter 23][], a small, freeware art program. Images can be pasted in and saved via Image -> Quick -> Quick GIF on the menu. It offers a series of colour options, and transparency (set by the bottom-left pixel)

![][painter23eg]

You might also want to check out [PaintShop Pro 3][] (dead link). This is just an old demo but - hey, it's not my fault that the time limit isn't enforced!

#Programs which can save animated .GIF files :

Coming Some Time! (Google till then ;) )

[PaintShop Pro 3]: http://www.natomic.com/hosted/marks/mpat/psp312-32.zip
[Painter 23]: http://www.natomic.com/hosted/marks/mpat/painter23.zip
[painter23eg]: /pixelart/images/mark_painter23eg.gif
