# Instructions pour les traducteurs

Merci pour votre intérêt à traduire Bazaar ! 🏷️🗺️💜

Ceci est une adaptation française de `TRANSLATORS.md` pour la traduction française spécifiquement.

Voici quelques règles simples:
* Vous devez parler couramment Français et avoir une bonne compréhension de l'anglais
* L'utilisation de modèles de langue (IA) pour générer du texte dans les fichiers est interdite (N'importe qui pourrait le faire). Si vous le faite, vous serez banni du projet par le créateur
* Le but n'est pas de fournir une traduction littérale, mais une expérience claire et complète aux utilisateurs francophones
* Utilisez `chore(po): update French translation` comme titre de Pull Request pour être uniforme avec les autres langues

> Note : La section "Recommandées" dépend du système dans lequel est fourni Bazaar - de plus il n'y a pas encore de système de traduction pour cette page là.

## Installation

> Pour traduire et soumettre votre traduction, il est recommandé de savoir utiliser Git, la plateforme Github et un éditeur de traductions comme [celui fourni par Gnome](https://flathub.org/fr/apps/org.gnome.Gtranslator) par exemple.

Vous devrez fork le projet et le cloner sur votre machine - afin de pouvoir soumettre votre traduction dans un Pull Request après.

Ensuite vous devrez vous assurez que le dossier actuellement ouvert est le dossier racine de Bazaar.

## Mise en place

### Automatique

Une fois que c'est fait, vous pourrez exécuter `./translators.sh` et suivre les différentes instructions (en anglais). Le script va vous montrer l'état actuel de `po/LINGUAS`. Si tout est correct, tappez `Y` et appuyez sur entrée.

Après cela, le script va vous demander votre code de langue (`fr`), écrivez le et appuyez sur entrée. Le script va générer un nouveau fichier `po` ou mettre à jour le fichier existant avec de nouvelles entrées traduisibles.

Vous êtes dorénavant prêts à ouvrir le fichier `po` dans votre éditeur de texte ou votre éditeur de traduction ([POEdit](https://flathub.org/apps/net.poedit.Poedit), [GTranslator](https://flathub.org/apps/org.gnome.Gtranslator), [Lokalize](https://flathub.org/apps/org.kde.lokalize), etc.) et à commencer à traduire.

Une fois que vous avez fini, envoyez vos commits et soumettez un Pull Request sur Github. Faites attention à n'envoyez que le(s) fichier(s) liés à votre traduction.

### Manuelle

Une fois que c'est fait, mettez en place meson avec le flag `im_a_translator` mis à `true`:

```sh
meson setup build -Dim_a_translator=true
```

Ensuite ouvrez le dossier `build`:

```sh
cd build
```

Et exécutez la commande suivante pour générer le fichier `pot` (**P**ortable **O**bject **T**emplate) principal :

```sh
meson compile bazaar-pot
```

Il se pourrait que vous verrez pas mal de messages disant que l'extension `blp` est inconnue - vous pouvez les ignorer.

Enfin, toujours dans le dossier build, exécutez la commande suivante pour mettre à jour ou créer le fichier `po` :

```sh
meson compile bazaar-update-po
```

Vous êtes dorénavant prêts à ouvrir le fichier `po` dans votre éditeur de texte ou votre éditeur de traduction ([POEdit](https://flathub.org/apps/net.poedit.Poedit), [GTranslator](https://flathub.org/apps/org.gnome.Gtranslator), [Lokalize](https://flathub.org/apps/org.kde.lokalize), etc.) et à commencer à traduire.

Une fois que vous avez fini, envoyez vos commits et soumettez un Pull Request sur Github. Faites attention à n'envoyez que le(s) fichier(s) liés à votre traduction.

## Mettre à jour la traduction existante

Générez à nouveau un fichier `.pot` (si nécessaire) à l'aide des commandes ci-dessus.

```sh
msgmerge --update --verbose po/fr.po po/bazaar.pot
```

Veuillez créer un commit séparé pour la mise à jour précédente dans votre Pull Request afin de faciliter la révision. Merci !

## Tester la traduction
Depuis le dossier de Bazaar, exécutez la commande suivante :

```sh
msgfmt po/fr.po -o bazaar.mo
```

Ensuite copiez le fichier `.mo` pour que Bazaar puisse le voir :
```sh
sudo cp bazaar.mo /var/lib/flatpak/runtime/io.github.kolunmi.Bazaar.Locale/x86_64/stable/active/files/fr/share/fr/LC_MESSAGES/
```

Assurez-vous d'abord de tuer le processus d'arrière-plan de Bazaar afin que les modifications/la langue souhaitées soient utilisées :
```sh
killall bazaar
```

Et ensuite redémarrez Bazaar pour voir vos traductions !

>Si vous n'avez pas votre système en français, vous pouvez utiliser la commande suivante pour lancer Bazaar en français:
>```sh
LANGUAGE=fr flatpak run io.github.kolunmi.Bazaar
```

## Notes des traducteurs

Les processus automatiques et manuels peuvent générer des entrées marquées comme « floues » (`fuzzy`).
Cela signifie que pour ces entrées, `gettext` a tenté de dériver une traduction préexistante.
Certaines suites de traduction, comme Lokalize, utilisent ce marquage pour définir les chaînes comme non révisées et les supprimer lorsque l'entrée est marquée comme terminée.
Lorsque vous travaillez avec des fichiers pot à l'aide d'un éditeur de texte, veillez à supprimer vous-même les marques `fuzzy` des entrées que vous considérez comme terminées, sinon votre traduction n'apparaîtra pas dans Bazaar.