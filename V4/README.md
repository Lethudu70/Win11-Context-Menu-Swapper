# Context Menu Swapper v4.1 — Leth Edition

Application Windows x64 en C++17 et Qt 6 Widgets, compilée avec MSYS2 **UCRT64**, CMake et Ninja.

## Version portable et interface

Extraire tout le ZIP de la release V4.1, puis lancer `ContextMenuSwapper.exe`. Garder les DLL à côté de l'EXE et les plugins dans leurs sous-dossiers, notamment `platforms/qwindows.dll`. Aucun Python ni MSYS2 n'est nécessaire sur le PC destinataire.

La fenêtre est arrondie, sans barre de titre Windows, avec boutons **−** et **×**. La déplacer par sa barre de titre ; double-cliquer dessus pour agrandir/restaurer. La poignée en bas à droite permet de redimensionner. L'interface propose le français, l'anglais, un thème sombre Leth et un thème système.

La fiche système lit le nom/édition de Windows, la version, la build avec sa révision UBR et l'architecture native. Le libellé historique « Windows 10 » du registre est corrigé en « Windows 11 » pour Windows client à partir de la build 22000.

## Mode standard : HKCU

1. Choisir **Menu classique** ou **Menu Windows 11**.
2. Cliquer **Appliquer la sélection**, puis confirmer.
3. L'application sauvegarde, effectue le changement et relit le registre.
4. Redémarrer Explorer via le bouton dédié, ou se déconnecter/reconnecter plus tard.
5. **Annuler le changement** restaure l'état précédant la dernière modification.

Clés ciblées :

```text
HKCU\Software\Classes\CLSID\{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}
HKCU\Software\Classes\CLSID\{51668040-5e31-4023-bbe3-149163bd80f1}
```

Le mode classique écrit une chaîne vide REG_SZ dans la valeur par défaut de `InprocServer32` sur les deux clés, en préservant les autres valeurs. Le retour au comportement Windows par défaut retire les deux clés CLSID après sauvegarde. Les clés existantes sont ouvertes avec le seul droit `KEY_SET_VALUE` pour l'écriture. La V4.1 utilise un seul snapshot pour les deux clés et accepte les snapshots à une clé de la V4.0.

Ce correctif vise le menu personnalisable de l’Explorateur de Windows 11 26H2 à partir du build **26300.9818** (KB5127936), dont **26300.9974** (KB5132061). Le contournement en HKCU a été confirmé manuellement par l’auteur sur un HP en 26300.9818. Microsoft documente le nouveau menu et les builds. La seconde clé est un contournement communautaire. [Notes Microsoft sur le menu](https://learn.microsoft.com/windows-insider/release-notes/release-preview-24h2-25h2/build-26200-9807-26300-9807) · [Historique des builds](https://learn.microsoft.com/en-us/windows/release-health/windows11-release-information) · [Signalement et retours de test](https://github.com/memstechtips/Winhance/issues/814)

Le snapshot JSON conserve sous-clés, types et octets des valeurs. Il est enregistré atomiquement dans `last-change.json` sous le dossier AppLocalDataLocation de l'application. Un verrou empêche deux instances de modifier l'historique utilisateur. Une nouvelle modification remplace l'annulation précédente. Les préférences INI sont gérées par QSettings dans le profil utilisateur.

Une restauration est refusée si la configuration a changé ailleurs. Une opération interrompue demande d'utiliser Annuler avant de recommencer. La sauvegarde HKCU ne conserve pas les ACL, propriétaires ou dates du registre. Éviter les modifications simultanées : le contrôle de concurrence n'est pas une transaction Windows.

## HKLM expérimental et UAC

**EXPÉRIMENTAL — NON RECOMMANDÉ AUX UTILISATEURS STANDARD.** L'option est décochée à chaque lancement. Elle cible les deux mêmes clés CLSID sous **HKLM**, dans la vue 64 bits, et peut affecter tous les comptes du PC. Un override HKCU peut rester prioritaire ; l'application ne le supprime pas automatiquement.

**Appliquer en HKLM… (UAC)** lance le même EXE via `runas` dans un processus séparé. Windows affiche le consentement UAC ou demande les identifiants d'un administrateur selon sa politique. L'application principale reste dans son contexte initial. Si elle était déjà élevée ou si l'UAC est désactivé, Windows peut ne pas demander une nouvelle autorisation.

Le helper accepte deux opérations fixes : activer l'override classique et restaurer l'état sauvegardé. Il exige un token élevé, une confirmation et une case de reconnaissance du risque. Il n'accepte aucun chemin de registre ou fichier de sauvegarde fourni par la ligne de commande. Un verrou global sérialise les opérations machine.

La sauvegarde machine est une valeur binaire `LastChange` sous :

```text
HKLM\Software\LethTools\ContextMenuSwapperV4\MachineBackup
```

Cette clé applicative est créée avec écriture réservée aux administrateurs et à SYSTEM, avec lecture pour les utilisateurs. Le snapshot lie la ruche, le chemin et la vue 64 bits. Les anciennes sauvegardes HKCU restent utilisables uniquement en HKCU.

### Valeur Windows protégée

Si Windows refuse encore l'écriture et si l'état initial est confirmé conservé/restauré, une seconde confirmation propose **Réessayer sur la clé protégée**. Une case de consentement, décochée au départ, autorise cette tentative avancée.

Le helper active temporairement **SeRestorePrivilege**, seulement si ce privilège lui est déjà attribué. Il ouvre la sous-clé existante avec `REG_OPTION_BACKUP_RESTORE`, puis rétablit immédiatement l'état précédent du privilège. Le handle sert à écrire uniquement la valeur par défaut ciblée. Le propriétaire et la DACL ne sont pas modifiés. Si le privilège est absent ou si Windows refuse encore, le diagnostic précise l'API, la cible et le code d'erreur. Cette voie avancée ne crée pas un nouveau chemin protégé.

Le snapshot marque `protectedWrite: true` après consentement pour permettre à **Annuler HKLM**, après UAC et confirmation, de réutiliser cette voie si nécessaire. Une inscription système préexistante est restaurée en place ; elle n'est pas supprimée aveuglément. Aucun processus TrustedInstaller/SYSTEM n'est lancé.

Sources Microsoft : [runas et UAC](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-shellexecuteinfoa), [vue fusionnée HKCU/HKLM](https://learn.microsoft.com/en-us/windows/win32/sysinfo/merged-view-of-hkey-classes-root), [ouverture pour restauration](https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regcreatekeyexw), [activation de privilèges](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-adjusttokenprivileges).

## Nettoyer JSON

**Nettoyer JSON…** retire les caches reconnus (`cache.json`, `state-cache.json`, `session-cache.json`, `diagnostic-cache.json`) et l'historique HKCU déjà annulé. Les sauvegardes actives restent disponibles par défaut.

L'option **Réinitialiser aussi l'annulation HKCU active** archive le JSON sous `archive/*.json.bak` avant de le retirer du chemin actif. L'annulation automatique correspondante n'est alors plus disponible. Les JSON d'annulation illisibles sont également archivés avant réinitialisation.

Les JSON inconnus, sous-dossiers, préférences, verrou et sauvegarde HKLM ne sont pas supprimés. Aucune suppression récursive n'est effectuée. Ce nettoyage ne corrige pas un refus d'accès du registre.

## Explorer et compatibilité

Explorer est redémarré uniquement à la demande. Ses fenêtres peuvent se fermer et la barre des tâches disparaître brièvement : terminer les copies avant de confirmer. Seul le shell de la session courante est ciblé ; les autres sessions devront être actualisées séparément. L'application ne démarre pas avec Windows et n'effectue aucun appel réseau au lancement.

Une écriture vérifiée ne prouve pas le comportement visuel du menu. L'override dépend de la build et des politiques Windows. Le retour « Windows par défaut » retire la configuration de cet outil ; il ne crée pas un menu Windows 11 sur Windows 10. Les essais en Sandbox/VM restent nécessaires pour les builds utilisées. Les exécutables ne sont pas signés.

## Compiler avec UCRT64

Installer si nécessaire depuis un terminal **MSYS2 UCRT64** :

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-qt6-svg mingw-w64-ucrt-x86_64-binutils
```

Puis depuis PowerShell dans `V4/` :

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-ucrt64.ps1
# Autre emplacement MSYS2 :
.\scripts\build-ucrt64.ps1 -UcrtRoot 'D:\msys64\ucrt64'
```

Le script configure CMake/Ninja en Release, compile, exécute CTest, déploie Qt et les dépendances UCRT64 transitives, teste le dossier avec un PATH réduit aux dossiers système et crée un ZIP dans `dist/`. Utiliser le compilateur, Qt et les outils du même préfixe UCRT64. Les icônes sont déjà fournies ; `scripts/make_icon.py` permet de régénérer l'icône avec Pillow et librsvg.

## Tests et diagnostics

`--self-test --report chemin.json` utilise des clés temporaires HKCU. `RegOverridePredefKey` redirige HKLM vers une clé HKCU uniquement dans le processus de test. Les snapshots, conflits, annulations, permissions et nettoyages JSON sont testés sans modifier le menu réel. Le test de privilège vérifie sa remise dans l'état initial ; avec un token qui le possède, il teste aussi l'écriture et l'annulation sur une clé de test protégée.

`--preview chemin.png` produit les captures normales, anglais/système, compactes et HKLM, puis teste l'interface. Il ne demande pas l'UAC et ne modifie pas le menu. **Exporter le diagnostic…** fournit la version de Windows, la portée, l'état et le journal.

Les tests automatisés ne remplacent pas une vérification du menu réel, de l'UAC et du redémarrage du shell dans une VM, ni les essais clavier et DPI 100/150/200 % sur les builds annoncées compatibles.

## Dépendances et sources

Les DLL Qt et tierces sont liées dynamiquement et conservent leurs licences. `licenses/` rassemble les notices disponibles dans l'installation UCRT64. La redistribution reste soumise aux obligations applicables, notamment celles de Qt LGPL. Le dépôt n'attribue pas encore de licence propre au code du projet.

Les versions historiques restent disponibles. Les sources de la v4.1 sont dans `V4/src`, les ressources dans `V4/resources` et les scripts dans `V4/scripts`. Les DLL et exécutables générés sont distribués via GitHub Releases.
