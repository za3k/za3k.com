import collections, math, os, random, string

players = ["Alice", "Bob", "Carol", "Dave", "Edna", "Frank", "Gertie", "Harold", "Ian", "Jenny"]
code_names_per_player = 3
operatives_per_player = 5

LETTERS = string.ascii_uppercase
FIRST_NAMES = sorted("Dmitry Alexander Maksim Ivan Anna Sofia Maria Anastasia Vlad Grigori".split()) # 10
LAST_NAMES = sorted("Ivanov Smirnov Petrov Sidorov Popov Orlov Makarov Andreev Volkov Titov".split()) # 10
CODE_NAMES = sorted("Dacha Ruble Sputnik Kopeck Commissar Babushka Kulak Matryoshka Ukase Zek Muzhik Knout Namestnik Rasputitsa Golubsty Kasha Kissel Knish Kulich Kholodets Kvas Medovukha Okroshka Oladyi Paskha Pelmeni Pirog Pirozhki Rassolnik Sbiten Vodka Shchi Solyanka Ukha Vatrushka Borscht Syrniki Nyet Stolichnaya Da".split())

random.shuffle(players)
directors = LETTERS[:len(players)]
code_names = sorted(random.sample(CODE_NAMES, code_names_per_player*len(players)))

names = ["{} {}".format(first, last) for first in FIRST_NAMES for last in LAST_NAMES]
real_names = {}
for code_name, real_name in zip(code_names, random.sample(names, len(code_names))):
    real_names[code_name] = real_name

half = len(players) // 2
retirement = math.floor(len(code_names)*2*0.75)
longest_code_name = max(len(x) for x in code_names)
longest_real_name = max(len(x) for x in real_names.values())

ok = False
while not ok:
    start_directors = collections.defaultdict(list)
    operatives = dict()
    for director in directors:
        operatives[director] = sorted(random.sample(code_names, operatives_per_player))
        for code_name in operatives[director]:
            start_directors[code_name].append(director)
    ok = True
    for code_name in code_names:
        #if len(start_directors[code_name]) >= half+1:
        #    ok = False
        if len(start_directors[code_name]) == 0:
            ok = False

for fn in list(LETTERS) + ["GM"]:
    try:
        os.remove("{}.txt".format(fn))
    except OSError:
        pass
for director, player in zip(directors, players):
    with open("{}.txt".format(director), "w") as f:
        f.write("{}, you are Junior Director {director} (out of {min}-{max}). Keep your director letter secret.\n".format(player, director=director, min=directors[0], max=directors[-1]))
        f.write("The following {} codenames work for you. You MAY tell other players this secret information.\n".format(operatives_per_player))
        for code_name in operatives[director]:
            f.write("  {}, birth name {}\n".format(code_name.upper().rjust(longest_code_name), real_names[code_name]))
        f.write("\n")
        f.write("The {} codenames are:\n".format(len(code_names)))
        f.write("    {}\n".format(" ".join([cn.upper() for cn in code_names])))
        f.write("The {} possible first real names are:\n".format(len(FIRST_NAMES)))
        f.write("    {}\n".format(" ".join(FIRST_NAMES)))
        f.write("The {} possible last real names are:\n".format(len(LAST_NAMES)))
        f.write("    {}\n".format(" ".join(LAST_NAMES)))
        f.write("\n")
        f.write("You may submit one report to the inspector by PM, for each codename.\n")
        f.write("- Submissions are one line each\n")
        f.write("- Your director letter MUST be the first thing on the line\n")
        f.write("- All reports are announced by your real name. Only the contents are secret\n")
        f.write("- ( 0 points) Report an incorrect real name\n")
        f.write("- (+2 points) Each honest (reported by {} or less directors) codename submitted with a correct real name\n".format(half))
        f.write("- ( 0 points) Each traitor (reported by {} or more directors) codename submitted with a correct real name\n".format(half+1))
        f.write("- (-1 points) Each honest codename you submitted, later found to be a traitor (reported by {} directors)\n".format(half+1))
        f.write("- ( 0 points) You can submit the word 'FAKE' instead of a report\n")
        f.write("\n")
        f.write("You don't know your point total. If you reach {} points, your retirement will be announced. Retired players can only submit FAKE reports.\n".format(retirement))
        f.write("The game ends if: one hour passes, all players but one retire, or at the inspector's discretion if things get boring.\n")
        f.write("The player with the most points at the end of the game wins.\n")
        f.write("\n")

with open("GM.txt", "w") as f:
    f.write("Directors\n")
    for director, player in zip(directors, players):
        f.write("{} - {}\n\n".format(director, player))
    f.write("\n")
    f.write("Operatives\n")
    for code_name in code_names:
        f.write("{}: {} -- {}\n".format(code_name.upper().rjust(longest_code_name) , real_names[code_name].ljust(longest_real_name), "".join(start_directors[code_name])))
