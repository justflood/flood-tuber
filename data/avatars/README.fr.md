# Package d’avatars Flood Tuber – Français

Ce package contient des dossiers d’avatars prêts à l’emploi pour le plugin OBS **Flood Tuber** (version améliorée par Poste2Travail).

## Utilisation
1. Installez Flood Tuber.
2. Décompressez cette archive.
3. Copiez les dossiers dans :
   `C:\ProgramData\obs-studio\plugins\flood-tuber\data\avatars\`
4. Dans OBS, ajoutez une source **Avatar Flood Tuber**, sélectionnez un avatar et cliquez sur **Charger et appliquer**.

## Nommage strict des fichiers
Le plugin recherche uniquement ces noms exacts :
- `idle.png` (obligatoire)
- `talk_a.png`, `talk_b.png`, `talk_c.png`
- `blink.png`, `action.png`
- Variantes optionnelles : `talk_a_blink.png`, etc.

## Créer ses propres avatars
Outils gratuits pour générer des personnages avec les expressions nécessaires :
- https://charat.me
- https://picrew.me

Exportez les différents états de bouche/yeux et renommez-les exactement comme indiqué.

## Outil de superposition
Utilisez le comparateur de sprites pour superposer vos fichiers et vérifier l’alignement / le crop avant importation.

Chaque dossier contient un exemple de `settings.ini`.
