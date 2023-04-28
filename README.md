# Unzip with `-I` and `-O` options

This repository offers gbp-compatible source tree to build unzip-i18n binary
package which offers `-I` and `-O` options like Ubuntu/Arch.

* `osamu`  branch is the default branch so this README.md is visible on the github.com page.
* `master` branch holds Debian changes residing only under `debian/` directory in compliance with gbp practices.
* `upstream` branch holds the upstream matching tar file found in `pristine-tar` branch.

gbp practice is the patch un-applied repository.


Since I changed the binary package name, upgrading unzip shouldn't override
your package selection.

## Git repo contents

This git repo is created from several upstream sources.

Debian packages are recorded into this repo using `gbp import-dscs --debnap`.

### Wikipedia on Info-ZIP
* https://en.wikipedia.org/wiki/Info-ZIP

### Upstream site:
* https://infozip.sourceforge.net/UnZip.html
* http://antinode.info/ftp/info-zip/

### Latest upstream (beta)
* https://sourceforge.net/projects/infozip/files/UnZip%206.x%20%28latest%29/

### Debian package tracker:
* https://tracker.debian.org/pkg/unzip

#### Debian bug report #779207:
* unzip fails to unpack filenames containing 'ä' 'ö' 'ü' -> results in "(invalid encoding)"
* https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=779207    (from 2015)
* https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=779207#15 (from 2017) pointer to patch

### Ubuntu package
* https://packages.ubuntu.com/search?keywords=unzip
* https://packages.ubuntu.com/lunar/unzip
* http://archive.ubuntu.com/ubuntu/pool/main/u/unzip/unzip_6.0-27ubuntu1.dsc

### Beyond Linux® From Scratch
* https://www.linuxfromscratch.org/blfs/view/svn/general/unzip.html




