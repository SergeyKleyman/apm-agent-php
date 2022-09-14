#!/usr/bin/env bash
set -xeo pipefail

release_tag="${1}"
original_packages_location="${2}"
downloaded_packages_location="./packages_downloaded_from_GitHub"

ls -l "${original_packages_location}"

echo "Downloading artifacts for tag \'${release_tag}\' to \'${downloaded_packages_location}\' ..."
mkdir -p "${downloaded_packages_location}"
pushd "${downloaded_packages_location}"

try_count=0
while [[ ${try_count} -lt 3 ]]; do
    ((++try_count))
    set +e
    gh release download "${release_tag}"
    exit_code=$?
    set -e
    if [ "${exit_code}" -eq "0" ]; then
        break
    fi
    sleep 10
done

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
