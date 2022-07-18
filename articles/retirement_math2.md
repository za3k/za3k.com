[za3k](/) > finances > retirement math, part 2: retiring for infinity years

In [part one](/articles/retirement_math1), I walked you through how people imagine the math works.

We imagined you made $60K/year, and spent $50K/year. You're 20, want to retire at 30, and plan to live to 80.
In part 1, we incorrectly concluded that you need to either:
- increase your income to $250K/year
- OR decrease your expenses to $10K/year

The instinct is right, but the numbers are wrong. To explain why, let me ask you an odd question. How much would you need to save to retire not until you're 80, but until you're 8,000? 80,000[^2]? You need to save up thousands of times more, right? No, because the math was *wrong*. You can do the above and you'll still be retired at 33.

### The math of retiring forever

- Suppose you make $60K a year, and you spend $20K a year. That means you're saving $40K per year.

- After 12.5 years, you will have saved up $500K.
- Assuming 4%[^3] stock market returns on that $500K, you are now making $20K/year from stock market investments.
- You are spending as much as you're making, so you're living on your investments.
- This means that you can now, in theory, stop working forever and live on your investments.
- Not retire for 10 years, not retire for 20 years--*forever*. If you live to 80,000[^2] you'll be fine, as long as the stock market continues to make about 4% a year.

That's right, money makes you money. If you live cheap and invest even below-average, you can retire forever at the age of 32.5.

Our original math said you can retire until the age of 80 at 30 using the same strategy. We can retire forever with 2.5 years extra? Is that right? Partly it is! But mostly the original math was incorrect. Our math now takes investing into account. As long as you want to live for infinity years (if you plan to die, read on).

### But what about me?

Well, what if you're not making $60K and spending $20K? I'll walk you through the general formula. You need to know three numbers

- Your yearly income (**I**)
- Your yearly expenses (**E**). *And*, these need to stay the same after you retire!
- How much you expect to make on the stock market, which I'm just going to set for you, at 4%[^3] yearly. Sometimes you'll see 0.04, sometimes 25[^4].

The rough[^5] estimate is that you can retire in: **25 x expenses / (income - expenses)** years.

Working through our example, that's = 25 * 20K / (60K/y - 20K/y) = 500K / 40K per year = 12.5 years. Imaginary-use is 20, so that works out to 32.5 years old. Checks out.

Okay, but what if... the stock market takes a hit?

[>> up and down](/retirement_math3.md)

[^2]: Which I am 100% in favor of. Get cracking on that science.
[^3]: This is very conservative. Since it's founding in 1957, the S&P 500 on average made 7% yearly over inflation. You can ajust the numbers if you want.
[^4]: Why 25? 25 is (1/0.04). You need 25 times any amount to make it at 4% per year. Want $100 per year? Invest $2500 at 4% per year.
[^5]: The rough estimate assumes you don't invest until you retire. This is mostly because the real answer involves a *lot* more math. If you invest all your money the whole time, you can retire in exactly (not roughly) 25 * ln(1+E/(I-E)) years. Good luck! You may need a calculator.
  For anyone who wants the derivation:
  ```
    > S(0)=0
    > S'(t)=0.04*S(t)+I-E
    ...differential equations is too hard, ask Wolfram Alpha...
    > S(t)=25*(I-E)*(e^0.04t-1)
    ...high school algebra, solve for S(t)=E/0.04...
    t=25 * ln(1+E/(I-E))
```
