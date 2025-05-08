import pystache
import subprocess

in_file = "MicroRPGs.csv"
out_file = "microrpgs.md"
template = "template.md"
max_players = 9

def csv2md(csv_text):
    return subprocess.check_output(["csv2md"], input=csv_text)

def run_csv_query(query):
    return subprocess.check_output(["q", "-H", "-d,", query])

if __name__ == '__main__':
    with open("template.md") as f:
        template = f.read()
    
    table = []

    for n in range(1, max_players+1):
        query = "select Game,Players,Time,Genre,Tone,Format,Players,Complexity from MicroRPGs.csv WHERE PlayersAll like \"%{}%\"".format(n)

        filtered_csv = run_csv_query(query)
        table.append({
            "markdown": csv2md(filtered_csv),
            "players": n,
        })

    out = pystache.render(template, {"table": table})

    with open("index.md", "w") as f:
        f.write(out)
