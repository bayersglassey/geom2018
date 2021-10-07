set -euo pipefail
. ip.sh

echo "### Updating repo..." >&2
ssh depths@$IP 'cd /srv/depths && git fetch && git reset --hard origin/master; git status'

echo "" >&2
echo "### Updating web assets..." >&2
rsync -v -a --delete -e ssh ./www/ depths@$IP:/srv/depths/www/

echo "" >&2
echo "### Adding symlink to repo in web assets..." >&2
ssh depths@$IP 'cd /srv/depths && ln -s $PWD www/repo'
