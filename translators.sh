#!/usr/bin/env bash
# Helper script that automates the preparation steps to translate Bazaar.
#
# The script will add the passed language code to the LINGUAS file to avoid
# editing it manually.
# also:
# It will reorder the contents of the file alphabetically;
# It will check if the code is already in the file;
# It will show the contents of the LINGUAS file, allowing a visual check;
# It won't really check if the locale code is valid but will inform you
# that it is not in the list of codes inside /usr/share/locale;
# It will generate a .backup inside /tmp before proceeding and a .tmp file
# before overwriting LINGUAS.
#
translators_helper() {

    local lang_input
    local lang_f
    local po_d
    local build_d
    local system_langs
    local langs_in_file
    local temp_file

    lang_f="LINGUAS"
    po_d="po"
    build_d="build"
    mapfile -t system_langs < <(find /usr/share/locale/ -maxdepth 1 -type d -printf "%f\n")
    temp_file="$(mktemp --suffix=BAZAAR)"
    printf "Temporary file: %s\n" "${temp_file}"

    printf "Setting im_a_translator to true\n"
    meson setup build -Dim_a_translator=true || return 1

    pushd "${po_d}" >/dev/null || return 1

    langs_in_file="$(wc --lines "${lang_f}" | grep --only-matching --extended-regex "[0-9]{1,}")"
    printf "\nNumber of languages currently in file %s\n" "${langs_in_file}"
    unset langs_in_file

    printf "Language codes currently in %s file.\n" "${lang_f}"
    cat --squeeze-blank "${lang_f}"
    read -r -n 1 -p "Proceed? (Type y or Y to confirm)" YN
    case "$YN" in
    [Yy])
        printf "\n%s\n" "Proceeding..."
        ;;
    *)
        printf "\n%s\n" "Leaving..."
        exit 3
        ;;
    esac

    printf "\nCopying %s to /tmp/%s\n" "${lang_f}" "${lang_f}.backup"
    cp --verbose "${lang_f}" "/tmp/${lang_f}.backup"

    read -r -p "Type the language code you want to enable translation for. ex.: pt_BR or es: " lang_input

    if [[ "${system_langs[*]}" =~ ${lang_input} ]]; then
        printf "Found %s in the system's language code list.\n" "${lang_input}"
        sleep 2s
    else
        printf "Could not find \"%s\" in the system's language code list, but proceeding nonetheless.\n" "${lang_input}"
        sleep 2s
    fi

    if grep --only-matching "${lang_input}" "${lang_f}"; then
        printf "%s already in file\n" "${lang_input}"
        sleep 2s
    else
        printf "Language to be added: %s\n" "${lang_input}"
        printf "%s\n" "${lang_input}" | tee -p --append "${lang_f}" 2>&1
        sleep 2s
    fi
    cat --squeeze-blank "${lang_f}" | (
        sed --unbuffered 1q
        sort
    ) | tee -p "${temp_file}"
    cat --squeeze-blank "${temp_file}" >"${lang_f}"

    langs_in_file="$(wc --lines "${lang_f}" | grep --only-matching --extended-regex "[0-9]{1,}")"
    printf "Number of languages currently in file %s\n" "${langs_in_file}"
    unset langs_in_file

    cat --squeeze-blank "${temp_file}"
    cat --squeeze-blank "${lang_f}"
    # $EDITOR LINGUAS
    popd || return 1

    pushd "${build_d}" || return 1

    printf "Generating the main pot (Portable Object Template) file for lang %s...\n" "${lang_input}"
    meson compile bazaar-pot

    printf "Update and/or create the po (Portable Object) files for %s.\n" "${lang_input}"
    meson compile bazaar-update-po

    echo "--------------------------------"
    printf "\nConfiguration done. Now ready for you to open your \"po\" file in your text editor and begin translating.\n"
    printf "When you are done, commit your changes form your fork and submit a pull request on \n%s also, refer to TRANSLATORS.md if needed\e]8;;\e\\ \n\n" "https://github.com/bazaar-org/bazaar/blob/master/TRANSLATORS.md"
    echo "--------------------------------"

    popd || return 1

    return 0
}

translators_helper || echo 'An error occurred; review the above output' 2>&1
