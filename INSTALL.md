# Jazz Jackrabbit — installation

Requires Retro-Go SD **v2.0.0-rc1 or later** on the Game & Watch.

The game data is not included: you need the files of Jazz Jackrabbit 1 for
DOS (full game, shareware episode or Holiday Hare: `BLOCKS.000`,
`LEVEL0.000`, `SPRITES.000`, `PANEL.000`, `*.PSM`...).

1. Build the data pack (Python 3, nothing else to install). Keep the
   `jj_render` folder next to `jj_pack.py`: it pre-decodes the pictures and
   renders the music.

       python3 jj_pack.py Jazz.jj /path/to/JAZZ

   On Windows:

       py jj_pack.py Jazz.jj C:\Games\Jazz

   The pack is about 32 MB (full game). Smaller: `--no-music` (about 18 MB).

2. Copy to the SD card, side by side, with the same name:

       /homebrews/Jazz.bin
       /homebrews/Jazz.jj

3. Start "Jazz Jackrabbit" from the Homebrew tab. The first start copies the
   pack to the external flash: it takes a few minutes.

Controls: D-pad = move (up = look up, down = crouch), A = jump (validate in
the menus), B = fire, TIME = change weapon, GAME = game menu / back,
PAUSE/SET = Retro-Go menu (savestates; Options: music, sound, swap A/B).
A savestate restarts its level from the last checkpoint reached, with the
score, lives and ammo of the moment.

---

# Jazz Jackrabbit — installation (français)

Nécessite Retro-Go SD **v2.0.0-rc1 ou plus récent** sur la Game & Watch.

Les données du jeu ne sont pas fournies : il faut les fichiers de Jazz
Jackrabbit 1 pour DOS (jeu complet, épisode shareware ou Holiday Hare :
`BLOCKS.000`, `LEVEL0.000`, `SPRITES.000`, `PANEL.000`, `*.PSM`...).

1. Créer le pack de données (Python 3, rien d'autre à installer). Garder le
   dossier `jj_render` à côté de `jj_pack.py` : c'est lui qui pré-décode les
   images et calcule la musique.

       python3 jj_pack.py Jazz.jj /chemin/vers/JAZZ

   Sous Windows :

       py jj_pack.py Jazz.jj C:\Jeux\Jazz

   Le pack fait environ 32 Mo (jeu complet). Plus petit : `--no-music`
   (environ 18 Mo).

2. Copier sur la carte SD, côte à côte, avec le même nom :

       /homebrews/Jazz.bin
       /homebrews/Jazz.jj

3. Lancer « Jazz Jackrabbit » depuis l'onglet Homebrew. Au premier
   lancement, le pack est copié dans la flash externe : cela prend quelques
   minutes.

Commandes : croix = déplacements (haut = regarder en haut, bas =
s'accroupir), A = sauter (valider dans les menus), B = tirer, TIME = changer
d'arme, GAME = menu du jeu / retour, PAUSE/SET = menu Retro-Go (sauvegardes ;
Options : musique, son, inverser A/B). Une sauvegarde reprend son niveau au
dernier point de passage atteint, avec le score, les vies et les munitions du
moment.
