[za3k](/) > [games](/mygames.md) > logic potions

a logic game for 2-4 players about brewing potions. players are gradually eliminated by LOSING. the last player standing wins. how long it takes per turn depends entirely on your group. a game is not many turns. if you like zendo, mao, or nomic, you may like this game.

i recommend checking out the **faster** and/or **simpler** variants immediately before first play.

materials: a deck of cards or a lot of skittles; a piece of paper and a pencil

**this game is unplaytested**

written by zachary "za3k" vance on 2021-03-13.

## overview
the players take turns combining ingredients to brew potions, and inventing rules about what happens when you combine ingredients. there are difficult logic goals for each half of the game. if you violate *any* of these six goals, you have *not* violated the rules of the game. however, the other players have the *opportunity* to call you out and make you immediately LOSE, if and when they catch you violating a goal.

first, you want to brew potions. to brew a potion, you combine ingredients and declare what potion type you get.
- the outcome should be legal under the current Rules (or a player will say **"ILLEGAL"**)
- even though you select a specific result, there should be at least two possible results under the Rules (or a player will **PREDICT** the only possible result)

second, you make Rules about how potion brewing works.
- any Rules you make should be compatible with previous Rules (or a player will issue a **Contradiction Challenge**)
- every set of ingredients needs to have a valid potion that can result (or a player will issue a **Contradiction Challenge**)
- any rule you make needs to actually *effectively change* the rules. this is done by always making your last brew illegal (or a player will say **"STILL LEGAL"**)
- you can't form a "theory of everything" where every possible brew can be predicted. (or a player will issue a **Universal Theory Challenge**)

for each goal, if a player violates the goal, you can call them out using the game rule in parentheses.

## play area and concepts
1. Ingredients
    - Grab a deck of ingredients, like a shuffled card deck with 4 suits, or a pile of skittles in 6 colors. Just use the 4 suits--you don't care about the numbers on the cards at all. 
    - Each player has a hand of ingredients. Draw 2 cards per player to start. Hands are face-up. 
    - In the middle of the table, there's a face-up "market" of 4 ingredients. Draw 4 ingredients at the start of the game to make the initial market.
    - The market never runs out. If the market is empty you can shuffle discards. If the market is still empty, add a maximum hand size or stop eating the skittles.
2. Potions List
    - At the top of your piece of paper, write some potion types. For your first game, write **"There are 3 potion types: red, fizzy, and tasty"**.
    - After the first game, make up your own types. Making up potion types is fun.
3. Rules
    - On the rest of the paper, *clearly and legibly* write numbered Rules.
    - The Rules go one below each other in numbered order. When you add a new Rule, write it at the bottom.
    - The name or initial(s) of the player who wrote the Rule goes next to the rule.
    - In this game, Rules are IF-THEN logic rules (see "Rules" below). Whenever a new Rule is added, write the IF and THEN parts of the Rule.
    - If you brew a potion, the result SHOULD match all Rules in play, and all Rules SHOULD be consistent with each other--but most of the game is about what to do when things aren't as they should be.
    - During a challenge, a blank piece of paper is temporarily used to cover up some of the rules.
    - **When a player LOSES, cross off every Rule they wrote** as no longer in play in the game.
4. Brewing
    - A brew is a set of ingredients (repeats are allowed) and a result (one of the potions from the potion list). For example "Two hearts and a spade make a fizzy potion". Ingredient order doesn't matter. There's only one potion type -- you make a "fizzy potion" or a "red potion" but never a "fizzy red potion".
    - Normal (real) brews are played from a player's hand of ingredients, after which the ingredients get used up.
    - During a challenge, "thought experiment" brews are performed, which don't need real ingredients from anyone's hand. Just pull some cards from the deck as a visual aid, and put them back after.
    - Brews aren't written down. 
    - This means new brew results and Rules are allowed to contradict old brew results. Sorry! See *Emperical Potions* in "Variants" below for a more scientific setup.

## rules
Example Rules

- Rule 1: IF a potion contains a heart, THEN it is *not* tasty.
- Rule 2: IF a potion contains *exactly* two ingredients, THEN it is fizzy.
- Rule 3: If a potion contains a heart or a spade, THEN it is red.

1. A Rule is always an IF-THEN clause with two parts: IF and THEN.
2. The IF clause refers only to the ingredients used to brew the current potion
3. The THEN clause refers only to the result of the current potion brew
4. Logical statements like "And", "Or", "Not", "At least one", "Exactly one", etc are fine. "Or" always means "if one or more of the following are true", never "if EXACTLY one of these are true".
5. I recommend you limit rules to be very simple. Rules should be clear, unambiguous English. They should be 20 words or less. Everyone should immediately agree on whether a brew obeys a rule with no debate, and not much thinking. The rules should be easy to think about individually--as a group they will still get hard quickly.

## turns
Each player takes turn being the active player. On their turn, the active player does the following in order. A player can always choose to LOSE and end their turn early.

1. The active player may optionally challenge an existing Rule, see below. They can issue any number of challenges, but each is resolved separately.
2. The active player selects 1-3 ingredients from their hand to brew. The goals of brews are given in "Overview" above.
    1. They wait for the other players to PREDICT. Any player other than the active player may PREDICT (but only one player). Whoever calls it first gets it. If no one wants to predict, play proceeds normally. You can use a timer, go around in a circle and have each player PREDICT or pass, or just informally ask "Anyone want to predict?".
    2. To PREDICT, another player says aloud out what potion type they know it will make. They should be 100% sure--this is for when you can 100% deduce the answer, not just guess.
    3. The active player brews together their ingredients and announces the result from the potion list. The result can be any potion type.
    4. Any player can say "ILLEGAL" if they think the brew violates a Rule. They point at the brew and any Rule. Again, if all players pass or a timer runs out, play proceeds.
        - If the brew and the Rule are compatible, the player who said "ILLEGAL" LOSES, because they were wrong and the brew was legal.
        - If the brew and the Rule are incompatible, the active player LOSES for violating that Rule.
    5. If there was a prediction
        - If the active player brewed what was PREDICTED, the active player has been successfully PREDICTED and LOSES immediately.
        - If the active player brews anything else, the predicting player failed, so they LOSE.
    6. The brew is complete.
3. The player makes up a Rule which would make the brew they just did illegal, and writes it into the rulebook.
    1. A Rule can be anything allowable in the right format. It should be short, clear and readable. The goals of rules are given in "Overview" above.
    2. Any player can say "STILL LEGAL" if they think the current brew is still legal under the new rule. They point to the brew and the new Rule. Again, if all players pass or a timer runs out, play proceeds.
        - If the brew and the Rule are compatible, the active player LOSES for failing to make the brew illegal under the new Rule.
        - If the brew and the Rule are incompatible, the player who said "STILL LEGAL" LOSES, because they were wrong and the brew is illegal now.
4. The active player "drinks" the brewed potion, discarding the brew ingredients.
5. The active player takes 2 ingredients from the market and adds them to their hand.
6. The active player draws replacement ingredients for the market, bringing it back up to 4 ingredients. 

## losing
- If you LOSE, you're immediately out of the game. If it's your turn, your turn is over.
- **All Rules you've ever written are immediately crossed off**
- The last player left wins.

## IF-THEN logic primer
Example Rules

- Rule 1: IF a potion contains a heart, THEN it is *not* tasty.
- Rule 2: IF a potion contains *exactly* two ingedients, THEN it is fizzy.
- Rule 3: If a potion contains a heart or a spade, THEN it is red.

1. In Logic Potions, a brew and a Rule can be compatible or incompatible. You will never need to determine if two Rules are compatible with each other, only a brew and a Rule.
2. If the IF part is false, the brew result and the Rule are compatible.
    - One spade makes a tasty potion: **compatible** with Rule 1. If a brew does NOT contain a heart, it is always compatible with Rule 1.
    - One heart makes a fizzy potion: **compatible** with Rule 2. If a brew contains 1 or 3 ingreidents, it is always compatible with Rule 2.
    - One heart and one spade make a fizzy potion: **compatible** with Rule 3. If a brew contains a heart or a spade, it is always compatible with Rule 3.
3. If the THEN part is true, the brew result and the Rule are compatible.
    - One heart makes a fizzy potion: **compatible** with Rule 1. If a brew result is not a tasty potion, it is always compatible with Rule 1.
    - One heart makes a fizzy potion: **compatible** with Rule 2. If a brew result is a fizzy potion, it is always compatible with Rule 2.
    - One heart and one diamond makes a red potion: **compatible** with Rule 3. If a brew result is red, it is always compatible with Rule 3.
4. Otherwise, the brew result is always **incompatible** with the Rule. Also, the IF part is true and THEN part is false. 
    - One heart makes a tasty potion: **incompatible** with Rule 1
    - One heart and one spade make an red potion: **incompatible** with Rule 2.
    - One heart and one diamond make a fizzy potion: **incompatible** with Rule 3.

## challenging rules
On their turn, a player can challenge a Rule as being invalid in some way.

1. The person issuing the challenge is called the **challenger**. 
2. There are two kinds of challenge: Contradiction or Universal Theory (or Redundancy, if playing with the optional rules). The challenger names the challenge type, and Rule number being challenged. ex. "Rule 10 is invalid, I issue a Contradiction Challenge."
3. During a challenge, slide a piece of blank paper to cover all higher-numbered rules. Ex. if Rule 10 is challenged, rules 1-10 remain visible, while rules 11 and above are covered and not visible. Later rules aren't used during a challenge.
4. The **author**'s initials are recorded by each Rule. The author of the challenged Rule must defend against the challenge. You can't challenge your own Rule.
5. If they author fails the challenge, they LOSE. If they pass the challenge, the challenger LOSES instead.
6. The author or challenger may give up halfway through the challenge and voluntarily LOSE, to save time.

The challenges are all 100% legitimate. That is, if the challenger is correct, they can 100% always win (if they play correctly). If the challenger is incorrect, the author can 100% always win (if they play correctly). If you don't see why, play a few games or think about it more.

### contradiction challenge
A challenger claims the challenged Rule is not logically compatible with earlier Rules, or that it prevents every possible potion result for some set of ingredients.
1. Thought Experiment: The challenger names a set of 1-3 ingredients (nobody needs to have these ingredients) with a contradictory result.
2. The author names any potion result. The ingredients and potion result form a brew.
3. The challenger points to the brew and any visible Rule.
    - If the brew and Rule are compatible, the author passes the challenge, and the challenger LOSES. 
    - If the brew and Rule are incompatible, the author fails the challenge and LOSES.

Example Contradictory Rules:

- Rule 1: IF a potion contains a spade, THEN it is fizzy
- Rule 2: IF a potion contains a heart, THEN it is red.
- Thought Experiment: What does one spade and one heart brew?

### universal theory challenge
A challenger claims the author has completed a Universal Theory of Everything, where the result of every possible brew can be logically deduced. The author must defend.
1. Thought Experiment: The author names a set of 1-3 ingredients (nobody needs to have these ingredients) they think is not possible to predict using the visible rules.
2. The challenger names any prediction (which they think is the only possible predicted result).
3. The author names any other brew result they think is possible, forming a brew.
4. The challenger points to the brew named and any visible Rule.
    - If the brew and Rule are compatible, the author passes the challenge and the challenger LOSES.
    - If the brew and Rule are incompatible, the author fails the challenge and LOSES.

Example Univeral Theory Rules:

- Rule 1: IF a potion contains two or more ingredients, THEN it is fizzy
- Rule 2: IF a potion contains two or less ingredients, THEN it is fizzy

### redundancy challenge (optional)
A challenger claims that a Rule is a logical consequence of earlier Rules (aka is redundant with them, aka can be "deduced" or proven from them). The challenger may quickly explain why, to see if the author agrees and gives up.
1. Thought Experiment: The author names a set of 1-3 ingredients (nobody needs to have these ingredients). The author also names two potion results C and I. These form brew C (ingredients + result C) and brew I (ingredients + result I). Make sure both players are clear on which is C and which is I.
    - One result, **C** the author thinks is compatible with Rules 1-9, **and also compatible with Rule 10**.
    - One result, **I** the author thinks is compatible with Rules 1-9, **but incompatible with Rule 10**.
2. If the challenger thinks the setup is invalid, they point at either brew and any visible Rule 1-9 (not the final one).
    - If the brew and Rule are compatible, the author passes the challenge, and the challenger LOSES.
    - If the brew and Rule are incompatible, the author fails the challenge and LOSES.
3. If the challenger thinks BOTH are compatible the Rules 1-10, the challenger points to brew I and to Rule 10 and says "both".
    - If the brew and Rule are compatible, the author fails the challenge and LOSES.
    - If the brew and Rule are incompatible, the author passes the challenge and the challenger LOSES.
4. If the challenger thinks NEITHER is compatible with Rules 1-10, the challenger points to brew C and to Rule 10 and says "neither".
    - If the brew and Rule are compatible, the author passes the challenge and the challenger LOSES.
    - If the brew and Rule are incompatible, the author fails the challenge and LOSES.

## variants
### faster
For a faster game. Whoever makes someone LOSE first wins, and the game is over.
### simpler
For a simpler set of rules. Challenging Rules is not allowed at all. Note, this game can end in a deadlock with good players.
### passing the buck
- If an author is challenged, partway through that challenge the author may "pass the buck", interrupting the process to themselves challenge a lower-numbered Rule. Whether they succeed or fail, the original challenge is abandoned. The original challenger may continue to challenge Rules as usual, including Rules by the original author.
- If the active player is brewing a potion and someone calls ILLEGAL, they are allowed to "pass the buck" with a Contradiction Challenge with those exact ingredients, mid-brew. If they succeed, they keep their ingredients and start their brew phase over.
### mathematical
STILL LEGAL is removed from the game, and Redundancy Challenges are added to the game in its place. This means new Rules no longer relate to the brew in any way.
### emperical potions
This is closer to real empericism (which I like), but it makes the game slower and more think-y (which I don't). This variation also requires the 'mathematical' variant, because Rules must always keep current and past brews legal.

- Rules are now called Hypotheses. Brews are called Experiments and get written in the rules list once a player completes them.
- During a challenge, thought experiments are not recorded as Experiments.
- When someone loses, only their Hypotheses are crossed off, not their Experiments.
- You can challenge a Hypothesis or an Experiment. If you challenge an Experiment, you must issue an Emperical Challenge or a Data Consistency Challenge.
- You can issue an Emperical Challenge on either a Hypothesis or an Experiment. Point to an Experiment and a Hypothesis. One of the two must be the challenged Rule. If they are consistent, you LOSE. If they are inconsistent, the author LOSES.
- You can issue a Data Consistency Challenge on an Experiment. Point to another visible (lower-numbered) Experiment. If they have identical ingredients but different results, the author LOSES. If the ingredients are not identical, or the results are the same, you LOSE.
### more potions
Rather than having potion types, you have potion adjectives. So you can now brew "a fizzy red potion". I haven't tested this. You could allow IF clauses to refer to adjectives.
### victory potions
For a more complicated game. Add a special potion type, a victory potion. If you finish brewing a victory potion (no one STEALS), you drink it. The game is over and you WIN.

If you brew a victory potion, someone else is allowed to shout out STEAL. They name another brew result instead, and take over your brew phase. If they finish brewing, you LOSE. If they LOSE before they finish brewing, you start your brew phase over.
### potion effects
When you drink each potion type, something happens. Decide what.

## history
- The original version of this game used what are now the "emperical potions" and "mathematical" variants.
- I added the mechanism wherein you cross off all Rules by players who LOSE. This prevents a problem where you can end up with an invalid ruleset and be unable to fix it, at the cost of making things slower.
- The "Emperical" rules were very slow and think-y, so I dropped the realism of empericism in favor of fast play with less record-keeping, for broader appeal. 
- I again dropped the mathematical appeal of "redundancy" challenges in favor of the broader appeal of STILL LEGAL.

## credits
This idea is loosely based on a game my mathematician friends came up with, but which I never played, called "Imaginary Go Fish". That game is written by mathematicians and intended for the same, and is based on formal theorem proving or metamathematics. I replaced theorems by Zendo-style specific examples or counterexamples, in keeping with an original emperical theme I dropped but hope to re-use in a later game. The examples does make it constructivist (I think?), which is actually a little weird in a deduction logic game--you can't use process of elimination on the potion types. The main changes from Imaginary Go Fish are the addition of the hand of ingredients, which I think makes the game more appealing and replayable; the gradual elimination of players in place of sudden death, which works better with some careless players; and the simplifications I made to remove 'redundancy' endgame for broad appeal. Also, credit to Zendo and a previous game by myself about regexes for the example/counterexample mechanisms.

For anyone curious, the rules of Mathematical Go Fish were something like:
- You start with four imaginary cards. 
- Each turn, you ask another player for a card. They can give you the card or say "go fish" and draw a card. Other than the cards being imaginary, it's just like normal Go Fish rules. Like Gin rummy, you don't discard sets until the very end.
- You win if you can prove you've won.
- You win if you can prove every card everyone is holding.
- You lose if you tell someone to go fish, and they can prove you have the card.
- You lose if you give someone a card and someone proves you don't have it.
- You win if you show someone lost.
