[za3k](/) > finances > retirement math, part 5: the math

In [part one](/articles/retirement_math1), I walked you through how people imagine the math works to retire early. We need to make a ton of money, or spend almost nothing.
In [part two](/articles/retirement_math2), I talked through retiring *forever*, using the real math. It only took a little longer than retiring until 80 with the fake math.
In [part three](/articles/retirement_math3), I explained about *snowballing up* and *snowballing down*, and why you might want to save more than the bare minimum.
In [part four](/articles/retirement_math4), I explained to the dumber readers that retiring for less time than forever, costs less money.

Okay, we're done now, right? Yes. We're done. Now we're going to tie everything together.

In part two, we revealed the rough formula to retire forever. **Y = 25 x E / (I - E)**. We used a very convervative stock market return of 4%. Easy-peasy.

Well actually, in the footnote of part two, we mentioned the exact formula: **Y = 25 * ln(1 + E/(I-E))**. Uh, that's a little worse, logarithms. Hmm.

In part three, we added a fudge factor. We want to save up 20% or 50% extra. We won't actually factor that in. Do it yourself.

From part four, we learned that to retire for Y years, we need **25 x E x (1 - e^(-0.04 * (80 - retirement_age)))** dollars, not **25 x E** like we claimed in part two. This is starting to look a little gross. Exponents?

Combining our two equations, we get: **Y = 25 x ln(1 + E/(I-E)) x (1-e^(-.0.04 * (80-20-Y)))**. That's not looking... great.

Actually, let's take out that rate of 4%, too. Someone could want to adjust that. **Y = (1/r) x ln(1 + E/(I-E)) x (1-e^(-r * (80-20-Y)))**.

And we can add variables for the two ages. Maybe we're not 20, or we don't plan to die at 80. We only use the difference (60 years), so we add **L** for life-expectancy: **Y = (1/r) x ln(1 + E/(I-E)) x (1-e^(-r * (L-Y)))**

We'll just solve for... well, uh. That looks kind of like a hot pile of garbage, huh? Five variables. You know what? Maybe we just back away slowly.

[[TODO: Add a javascript calculator? Volunteers?]]
