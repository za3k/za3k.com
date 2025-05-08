import pystache
import subprocess

max_players = 9

def csv2md(csv_text):
    return subprocess.check_output(["csv2md"], input=csv_text)

def run_csv_query(query, csv_text):
    return subprocess.check_output(["q", "-H", "-O", "-d,", query], input=csv_text.encode("utf8"))

if __name__ == '__main__':
    with open("template.md") as f:
        template = f.read()
    with open("MicroRPGs.csv") as f:
        csv = f.read()
    
    table = []

    for n in range(1, max_players+1):
        query = "SELECT Game,Players,Time,Genre,Tone,Format,Complexity from MicroRPGs.csv WHERE PlayersAll LIKE \"{n},%\" OR PlayersAll LIKE \"%,{n},%\" OR PlayersAll LIKE \"%,{n}\"".format(n=n)

        filtered_csv = run_csv_query(query, csv)
        table.append({
            "markdown": csv2md(filtered_csv),
            "players": n,
        })

    out = pystache.render(template, {"table": table})

    with open("../index.md", "w") as f:
        f.write(out)
