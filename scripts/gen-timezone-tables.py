#!/usr/bin/env python3
"""Generate src/gen/geo-tz-tables.inc, the place name -> IANA timezone table,
and src/gen/geo-charmap.inc, the character folding used to normalize names,
from GeoNames data (https://www.geonames.org, CC BY 4.0).

Usage: gen-timezone-tables.py [--dumps DIR]

The GeoNames dumps (cities15000, admin1CodesASCII, countryInfo and the ~200MB
alternateNamesV2) are downloaded into DIR (default .cache/geonames) unless
already there, so rerunning is cheap and refreshing is a matter of deleting
the directory. Only the generated files are committed.

Entries are cities (GeoNames name, ASCII name, and alternate names in English
and in the languages of the country), first-level administrative divisions
(states, provinces...) and countries. A division takes the timezone covering
the largest share of its population, a country the timezone of its capital.
Divisions and countries carry a population so that a bare name resolves to
the most populous thing called that; ISO country codes and division codes
("tx", "fr") are only usable as qualifiers ("paris tx").

Names are lowercased and folded to ASCII when they can be (Zürich -> zurich,
München -> munchen), kept as lowercase UTF-8 otherwise (Москва, 北京). Names
shorter than 3 bytes are dropped unless GeoNames flags them as a short name
(uk, la), and words the calculator grammar uses are dropped everywhere so that
"now in london" never resolves "in".
"""
import csv
import sys
import unicodedata
import urllib.request
import zipfile
import zoneinfo
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DUMPS = "https://download.geonames.org/export/dump/"
DUMP_FILES = ["cities15000.zip", "admin1CodesASCII.txt", "countryInfo.txt", "alternateNamesV2.zip"]
OUT = ROOT / "src/gen/geo-tz-tables.inc"
CHARMAP_OUT = ROOT / "src/gen/geo-charmap.inc"

CHUNK_SIZE = 16000
MIN_NAME_LEN = 3

# operator/keyword tokens of the parser; a place name equal to one of these
# would be tried as a timezone right after every date literal
STOPWORDS = {
    "in", "to", "as", "of", "at", "by", "on", "or", "and", "is", "it", "ago", "per", "from",
    "add", "plus", "minus", "mul", "div", "mod", "modulo", "pow", "power",
}

FOLD = str.maketrans({
    "ø": "o", "Ø": "o", "æ": "ae", "Æ": "ae", "œ": "oe", "Œ": "oe", "ß": "ss", "ł": "l", "Ł": "l",
    "đ": "d", "Đ": "d", "ð": "d", "Ð": "d", "þ": "th", "Þ": "th", "ı": "i", "ħ": "h", "Ħ": "h",
    "ŧ": "t", "Ŧ": "t", "ĸ": "k", "ŋ": "n", "Ŋ": "n",
})


def fold_char(c):
    """ASCII form of a character, None when it has none (Cyrillic, CJK...)."""
    s = unicodedata.normalize("NFKD", c.translate(FOLD))
    s = "".join(ch for ch in s if not unicodedata.combining(ch))
    return s if s.isascii() else None


def char_repl(c):
    """Foldable characters become ASCII, the rest is lowercased; dots and
    apostrophes vanish, any other non alphanumeric is a word separator."""
    if c in ".'’":
        return ""
    folded = fold_char(c)
    if folded is None:
        folded = c.lower()
    return "".join(ch.lower() if ch.isalnum() else " " for ch in folded if ch not in ".'")


def normalize(name):
    """Mirror of normalizeName() in src/unicode.cpp."""
    return " ".join("".join(char_repl(c) for c in name).split())


def cstring(s):
    """Octal rather than hex escapes: hex would swallow the digits that follow."""
    out = []
    for b in s.encode("utf-8"):
        if 0x20 <= b < 0x7F and b not in (0x22, 0x5C):
            out.append(chr(b))
        else:
            out.append(f"\\{b:03o}")
    return "".join(out)


raw_names = set()


def variants(name):
    """"The Netherlands" is also looked up as "netherlands"."""
    raw_names.add(name)
    norm = normalize(name)
    if not norm or norm in STOPWORDS:
        return []
    out = [norm]
    if norm.startswith("the ") and len(norm) > 4:
        out.append(norm[4:])
    return out


def read_tsv(path):
    with open(path, newline="", encoding="utf-8") as f:
        return [row for row in csv.reader(f, delimiter="\t", quoting=csv.QUOTE_NONE) if row]


def fetch(raw):
    raw.mkdir(parents=True, exist_ok=True)
    for name in DUMP_FILES:
        path = raw / name
        if not path.exists():
            print(f"downloading {DUMPS}{name}")
            urllib.request.urlretrieve(DUMPS + name, path)
        if path.suffix == ".zip" and not path.with_suffix(".txt").exists():
            zipfile.ZipFile(path).extract(path.with_suffix(".txt").name, raw)


def load(raw):
    cities = [(r[0], r[1], r[2], r[8], r[10], r[14], r[17]) for r in read_tsv(raw / "cities15000.txt")]
    admin1 = [r[:4] for r in read_tsv(raw / "admin1CodesASCII.txt")]
    countries = [(r[0], r[4], r[5], r[7], r[16]) for r in read_tsv(raw / "countryInfo.txt")
                 if not r[0].startswith("#")]

    # local names (München) live in the alternate names of the country's languages
    languages = {r[0]: {lang.split("-")[0] for lang in r[15].split(",") if lang}
                 for r in read_tsv(raw / "countryInfo.txt") if not r[0].startswith("#")}
    country_of = {r[0]: r[3] for r in cities}
    country_of.update((r[3], r[0].split(".")[0]) for r in admin1)
    country_of.update((r[4], r[0]) for r in countries)

    alt_names = defaultdict(list)
    with open(raw / "alternateNamesV2.txt", encoding="utf-8") as f:
        for line in f:
            r = line.rstrip("\n").split("\t")
            iso = country_of.get(r[1])
            if iso and r[2] and (r[2] == "en" or r[2] in languages.get(iso, ())):
                alt_names[r[1]].append((r[3], r[5] == "1"))
    print(f"{len(cities)} cities, {len(admin1)} admin1, {len(countries)} countries, "
          f"{sum(map(len, alt_names.values()))} alternate names")
    return cities, admin1, countries, alt_names


def generate(raw):
    cities, admin1_rows, country_rows, alt_names = load(raw)

    tz_names = sorted({c[6] for c in cities if c[6]})
    tz_index = {tz: i for i, tz in enumerate(tz_names)}
    known = zoneinfo.available_timezones()
    for tz in tz_names:
        if tz not in known:
            print(f"warning: {tz} unknown to the local tz database", file=sys.stderr)

    # each country gets a synthetic admin1 for cities without one and for itself
    countries = {}
    for iso, name, capital, population, gid in country_rows:
        countries[iso] = {"idx": len(countries), "name": name, "capital": capital,
                          "population": int(population or 0), "gid": gid}

    admin1 = {}
    for iso in countries:
        admin1[(iso, "")] = {"idx": len(admin1), "country": iso, "names": [], "code": ""}
    for code, name, ascii_name, gid in admin1_rows:
        iso, sub = code.split(".", 1)
        if iso not in countries:
            continue
        names = [name, ascii_name] + [n for n, _ in alt_names.get(gid, [])]
        admin1[(iso, sub)] = {"idx": len(admin1), "country": iso, "names": names, "code": sub}

    admin1_tz_pop = defaultdict(lambda: defaultdict(int))
    country_cities = defaultdict(list)
    entries = []  # (normalized name, population, tz index, admin1 index)

    def add_entry(name, population, tz, adm, short=False):
        for norm in variants(name):
            if len(norm.encode("utf-8")) >= MIN_NAME_LEN or short:
                entries.append((norm, population, tz, adm))

    for gid, name, ascii_name, iso, sub, population, tz in cities:
        if not tz or iso not in countries:
            continue
        key = (iso, sub) if (iso, sub) in admin1 else (iso, "")
        population = int(population or 0)
        admin1_tz_pop[key][tz] += population
        country_cities[iso].append((name, ascii_name, population, tz))
        for n, short in {(name, False), (ascii_name, False)} | set(alt_names.get(gid, [])):
            add_entry(n, population, tz_index[tz], admin1[key]["idx"], short)

    for key, tz_pop in admin1_tz_pop.items():
        if not key[1]:
            continue
        tz, population = max(tz_pop.items(), key=lambda kv: kv[1])
        for n in admin1[key]["names"]:
            add_entry(n, sum(tz_pop.values()), tz_index[tz], admin1[key]["idx"])

    skipped = []
    for iso, c in countries.items():
        found = country_cities.get(iso)
        if not found:
            skipped.append(iso)
            continue
        capital = [x for x in found if c["capital"] and c["capital"] in (x[0], x[1])]
        tz = (capital or [max(found, key=lambda x: x[2])])[0][3]
        c["tz"] = tz
        for n, short in {(c["name"], False)} | set(alt_names.get(c["gid"], [])):
            add_entry(n, c["population"], tz_index[tz], admin1[(iso, "")]["idx"], short)
    if skipped:
        print(f"no city >= 15000 for {len(skipped)} countries, skipped: {' '.join(sorted(skipped))}")

    qualifiers = set()
    for iso, c in countries.items():
        for n in [c["name"], iso] + [n for n, _ in alt_names.get(c["gid"], [])]:
            for norm in variants(n):
                qualifiers.add((norm, c["idx"], True))
    for key, a in admin1.items():
        if not key[1] or key not in admin1_tz_pop:
            continue
        for n in a["names"] + ([a["code"]] if a["code"].isalpha() else []):
            for norm in variants(n):
                qualifiers.add((norm, a["idx"], False))

    entries = sorted(set(entries))
    qualifiers = sorted(qualifiers)

    names = sorted({e[0] for e in entries} | {q[0] for q in qualifiers})
    chunks, offsets = [[]], {}
    used = 0
    for n in names:
        size = len(n.encode("utf-8")) + 1
        if used + size > CHUNK_SIZE:
            chunks[-1].append("\\000" * (CHUNK_SIZE - used))
            chunks.append([])
            used = 0
        offsets[n] = (len(chunks) - 1) * CHUNK_SIZE + used
        chunks[-1].append(cstring(n) + "\\000")
        used += size

    # every non-ASCII character of the source names, in every case, so that
    # src/unicode.cpp normalizes queries exactly like normalize() did the names
    char_map = {}
    for c in {c for name in raw_names for c in name if not c.isascii()}:
        for variant in {c, c.lower(), c.upper(), c.title()}:
            if len(variant) != 1 or variant.isascii():
                continue
            repl = char_repl(variant)
            if repl != variant:
                char_map[variant] = repl

    lines = [
        "// Generated by scripts/gen-timezone-tables.py from GeoNames (cities15000).",
        "// Do not edit by hand.",
        f"constexpr uint32_t kGeoNameChunkSize = {CHUNK_SIZE};",
        "constexpr std::string_view kGeoNameChunks[] = {",
    ]
    for chunk in chunks:
        lines.append(f'    "{"".join(chunk)}"sv,')
    lines += ["};", "", "constexpr std::string_view kGeoTzNames[] = {"]
    lines += [f'    "{tz}",' for tz in tz_names]
    lines += ["};", "", "constexpr uint16_t kGeoAdmin1Country[] = {"]
    for a in sorted(admin1.values(), key=lambda a: a["idx"]):
        lines.append(f'    {countries[a["country"]]["idx"]},')
    lines += ["};", "", "constexpr GeoEntry kGeoEntries[] = {"]
    for name, population, tz, adm in entries:
        lines.append(f"    {{{offsets[name]}, {population}, {tz}, {adm}}},")
    lines += ["};", "", "constexpr GeoQualifier kGeoQualifiers[] = {"]
    for name, ident, is_country in qualifiers:
        lines.append(f"    {{{offsets[name]}, {ident}, {'true' if is_country else 'false'}}},")
    lines += ["};", ""]
    OUT.write_text("\n".join(lines))

    lines = [
        "// Generated by scripts/gen-timezone-tables.py from GeoNames (cities15000).",
        "// Do not edit by hand.",
        "constexpr CharMapping kCharMap[] = {",
    ]
    for c in sorted(char_map):
        lines.append(f'    {{0x{ord(c):04X}, "{cstring(char_map[c])}"}},')
    lines += ["};", ""]
    CHARMAP_OUT.write_text("\n".join(lines))
    print(f"{len(entries)} entries, {len(qualifiers)} qualifiers, {len(names)} names in "
          f"{len(chunks)} chunks, {len(tz_names)} zones -> {OUT}")
    print(f"{len(char_map)} character mappings -> {CHARMAP_OUT}")


if __name__ == "__main__":
    raw = Path(sys.argv[2]) if sys.argv[1:2] == ["--dumps"] else ROOT / ".cache/geonames"
    fetch(raw)
    generate(raw)
