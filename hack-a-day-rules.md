<style>
img {
    margin-left: auto;
    margin-right: auto;
    border: 1px solid lightgrey;
    border-radius: 15px;
    width: 200px;
    height: 200px;
}
body {
    max-width: 30%;
    margin-left: 20%;
}
</style>

[za3k](/) > hack-a-day

Hack-A-Day is a "hackathon" or "game jam" style challenge, in the same vein as NaNoWriMo, no-shave November, and many others. The idea is to make one new project each day (and optionally, post them somewhere for people to see).

Last year (2022), I set myself the challenge to make a software project every day, and met it. I had a ton of fun, and make a lot of [cool video games and projects](https://za3k.com/hackaday) I can show off. This year I'm inviting the rest of the world to join me!

I'm a programmer, so I'm doing a new computer programming project every day. But I see no reason you can't do other kinds of projects.

## Last year's journey
I like doing challenges. I've tried [National Novel Writing Month (NaNoWriMo)](https://nanowrimo.org/) twice (and failed both times). Every november is no-shave november. For whatever reason, November feels to me like *Challenge Month*. So when november rolled around last year, I asked myself what challenge I wanted to do.

I had previously heard about another programmer (I think Remy Sharp?), challenging themselves to do one project a day for a month, or a year, or something like that. And I had always been really impressed at their daily projects, which if I remember right included some stuff like [JSBin](https://jsbin.com/) and maybe [Underscore templating](https://underscorejs.org/#template). Do I have all that information right? Probably not! But it's okay if your heroes are slightly fictional, I was not so concerned with what someone else had actually done or not. It just sounded like a cool story.

So I set myself a challenge: make a project every single day, from scratch, in November. And because I was a programmer, every project would be a software project. I wanted to show them off, so I'd at least start with some web projects, so I could give people a link and say "ooh look at this thing I made".

I'm retired, so I have a lot of time on my hands. I figured I could do a project in an average day! But I also wanted to avoid burnout, so I decided to aim a little smaller, and try to get them done in 3 hours or so, and have plenty of time for other things Now there was a small problem, which was that I started on Nov 3, so I had a little catching up. But I would worry about that later.

The first thing I did was think about how to write projects. I decided I would write them in Flask, a python webserver, so I people could go visit an interactive website. Now, would that actually be fun to write every day? No. So maybe I should spend the first day writing a framework to make the other days easier. But that sounded boring, and abstract, and like I might not know what to do. So instead I just make a very simple app, and decided I would figure out how to make the framework on the second day, based on day 1 (this never happened, btw). My first project was the simplest thing I could think of: an [online blog](https://tilde.za3k.com/hackaday/blog/). I would share it with friends, and they could all post on the blog if they wanted. At the last minute, I added some colors on impulse, which made it way nicer.

[![Hack-A-Blog](https://za3k.com/img/hackaday03.png)](https://tilde.za3k.com/hackaday/blog/)

From this, I learned a few things. First, I really love the delighted responses I got from the people I shared it with. People posting on it made me really happy. As the month went on, I started making more and more interactive things. And second, the five minutes of work I did at the end to make it pretty was most of the "cool" factor! So I tried to keep in pretty colors and styling as best I could (though I'm no artist).

The next day I wrote a simple chat program, to try and cover the technical parts (AJAX) I hadn't the first day. I took the first project, tried to separate the "blog" bits from the "just a website" bits, and just copied it. It turns out I kept doing that--I never really wrote a separate framework. It worked fine, and it turned out to have some positives, because I didn't keep tinkering with some library all day instead of my actual project. (Of course there were some downsides too, like 30 slightly different versions of the same code!)

[![Hack-A-Chat](https://za3k.com/img/hackaday04.png)](https://tilde.za3k.com/hackaday/chat/)

The third day I played catchup. I wrote three projects in one day, each taking about an hour. They were are really lame, and I kind of regretted it afterwards. I would rather have just written one cool project, and ended up with 30 projects.

The fourth day was my first failure. I checked out something called the [zero hour game jam](http://0hgame.eu/). In the USA, we move clocks twice a year to keep in sync with the seasons. One of those times, that means it's 2am, and then an hour later it's 2am again! That surely means 0 hours have passed in between. So some clever people decided to do a 0-hour game jam, where they make computer games in 0 hours. I was delighted, and one of the participants posted to youtube a video of them making a game in 1 hour. They used Unity, which I had tried to learn a few times and failed.

*Well!* I thought to myself. *If they can make a game in 1 hour, surely I can just exactly copy what they do in one whole day.* Oh boy, was I wrong. I got nowhere near done. But at the end of the day, I had Unity installed (mostly) and had gotten some ice cubes to appear on screen and fall. I wouldn't call it a game by any means, but it was kilometers ahead of anything I had gotten to work in Unity before, so I called it a huge success (even if I was secretly a little disappointed).

On day 5, I thought to myself. Should I finish the project from the day before? No, I decided. If I kept working on failures, I'd spend all my time on the projects I was worst at. Instead, I'd just fail and learn from it. But I still wanted to make a game in Unity, so I did. I wrote an *Asteroids* clone, where you fly around a spaceship and shoot asteroids.

[![Hack-An-Asteroid](https://za3k.com/img/hackaday07.png)](https://tilde.za3k.com/hackaday/asteroid)

I directly used a bunch of things I had done the day before! Not only had I installed Unity, but I had found a helpful community who helped me answer Unity questions, and a neat sound generator I used. And watching someone else make a game made me realize that adding background music and sound effects really added a lot to the game.

As the days went by, I started making cooler and cooler projects each day, despite not spending any more time of them. Some of that was learning--I learned nice libraries to use, how to do certain things in Javascript like mouse drag, and I had to look up less. Some of it though, was general project skill. I started learning which features to cut, and which were worth spending time on. Mouse AND keyboard controls? Nope, only need one. Sound effects when you shoot and particle effects? A must-have. Rounded borders and good spacing? Worth the time. Tutorials? Nope, they're great but take too long. Just write instructions at the bottom, we've only got 3 hours!

I also started slowly but surely getting burnt out. I needed a break from hacking every day. I couldn't really schedule time to hang out with my friends. So I took a day off for a holiday, and I felt great after. Then later, I took two days off--and completely lost my momentum! Oops, lesson learned the hard way. No taking two days off in a row. Too much got unloaded from my brain.

The other thing that surprised me was that I started learning what kind of projects I *wanted* to make. I thought I knew, but actually it turned out I learned a lot. I liked making projects I could show off, that made people go "Wow!". My favorite projects were things things where people immediately said "Cool!" and smiled. I like making people happy. 

My absolute favorite hack of the month was "Hack-A-Tank". Some little fish followed around your mouse. I don't know why, but it was relaxing as heck, and it made me really happy when other people felt the same way. Mood pieces were very outside my comfort zone, and I felt valid when people appreciated it.

[![Hack-A-Tank](https://za3k.com/img/hackaday17.png)](https://tilde.za3k.com/hackaday/tank)

Technically impressive stuff I knew from the start was meh. I thought I wanted to learn and experiment--and don't get me wrong, that was good. I'm glad I learned how to do voice chat. But making crowd-pleasers was even better. My other favorite was anything where I got "players" or "users". Seeing what people did with my toys was fun. My favorite in that respect was "Hack-An-Adventure", which read like a coloring book for kids. I gave people prompts like "you ride your loyal steed into town. Draw your loyal steed." And then they'd draw their steed, and relax, and I'd get to look at their adventure after.

[![Hack-An-Adventure](https://za3k.com/img/hackaday29.png)](https://tilde.za3k.com/hackaday/adventure)

All in all, I had a great month. I tried to keep it going for a while after, but it just didn't feel the same. My brain knew it wasn't November, and it petered out pretty fast. I also tried improving old projects, but I decided that would just be a way to have November give me a huge pile of work for the rest of the year. I picked *one* project, and make it better, but the rest I just left as-is, in the spirit of game jams.

[![Doodle RPG](https://za3k.com/img/hackaday30.png)](https://doodle-rpg.com/)

Since I had a great time, I plan to do it again this year. And I hope you'll all join me! The only thing more fun than a hackathon, is a hackathon with friends.

## Rules

Here is how the challenge works:

- *In November 2023,*
    - **The challenge is in November 2023.** You're allowed to start late.
- *do one project a day,*
    - **You make at least 20 projects.** Get out a calendar and schedule time!
    - **You have to make an entire project, start to finish in one day.** You don't have to use the whole day. 1-4 hours a day should be enough time.
    - **Pick a broad theme. For Hack-A-Day, your projects can be anything inside that theme.** Make it broad! "Art", not "painting". "Software", not "video games". "Making stuff", not "carpentry".
    - **You are not allowed to spend more than 1 day on a project.**
    - **You are not allowed to improve projects after the day is over.** Even after Hack-A-Day is over.
- *from scratch.*
    - **You are not allowed to work on existing projects.** (Programmers, that means NO bug fixes for an open-source project, adding features, or finishing up old stuff you have laying around. On the other hand, using libraries or frameworks is allowed and encouraged. You can even use ones you wrote *as long as you don't tinker on the libraries during Hack-A-Day*.)
    - **You are not allowed to work on or plan your project before the day.**
- *Challenge yourself*
    - **You should work on projects challenging to do in one day.**
- *but pace yourself*
    - **You are allowed to skip days.** But **you are not allowed to skip two days in a row**, or more than 10 days total.
    - **You do not have to successfully finish each project.** Only take on projects you *could* finish, but it's okay if you don't. It's better to try something fun and get 80% done, than to every day finish something lame you can definitely do in an hour.
    - **You can cheat a little!** The most important thing is to keep up the flow and stick to the spirit.
- *and have fun!*
    - **You should work on projects fun to do, fun to learn, or fun to show off.** Don't just pick something you can do, learn, or show off.
    - **You *can* share your projects if you want.** Feel free to share on social media of your choice, a blog, just show your friends, or to email me!

You can ignore any of the above rules if you don't like them. I'm not the boss of you! But since I am the only person that's done this challenge, I think I have good advice.

If you finish I will send you a little picture of a "2023 Hack-A-Day Complete" gold medal for being so cool and add your name to a list. I'll probably come up with a participation trophy too.

## Advice

Here is some additional *advice* from me.

- Tell people you're doing Hack-A-Day, and share your projects! It's fun, you get great feedback, and it's good accountability. But don't spend much time doing it.
- Try to make projects with a core and "stretch" goals. That way if you get 50% or 80% done with what you wanted, it will still be something cool.
- Don't skip 2 out of 3 days. You'll lose momentum.
- Brainstorm future projects every once in a while.
- Pay attention to which projects you're finding fun or not. I learned a lot about what makes me tick.
- Be flexible. Don't pick projects ahead of time. Go where your explorations take you.
- Set aside time for Hack-A-Day. Set aside time for *not* hacking, and hanging with friends or playing videogames :).
