#!/usr/bin/env bash
set -xeo pipefail

release_tag="${1}"
original_packages_location="${2}"
downloaded_packages_location="./packages_downloaded_from_GitHub"

ls -l "${original_packages_location}"

echo "Downloading artifacts for tag \'${release_tag}\' to \'${downloaded_packages_location}\' ..."
mkdir -p "${downloaded_packages_location}"
pushd "${downloaded_packages_location}"

this_script_dir="$( dirname "${BASH_SOURCE[0]}" )"
"${this_script_dir}/run_command_with_timeout_and_retries.sh" --timeout=10 --max-tries=3 --wait-time-before-retry=10 --retry-on-error=yes -- gh release download "${release_tag}"

ls -l .
echo 'Verifying that downloaded artifacts pass the downloaded checksums...'
sha512sum --check *.sha512
popd

sort "${original_packages_location}"/*.sha512 > original_artifacts.sha512
sort "${downloaded_packages_location}"/*.sha512 > downloaded_artifacts.sha512
cat original_artifacts.sha512
cat downloaded_artifacts.sha512

echo 'Verifying that original and downloaded artifacts have the same checksums...'
diff original_artifacts.sha512 downloaded_artifacts.sha512 || exit 1
