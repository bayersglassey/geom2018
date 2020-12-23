. ip.sh
ssh depths@$IP 'cd /srv/depths && git fetch && git reset --hard origin/master; git status'
rsync -v -a --delete -e ssh ./www/ depths@$IP:/srv/depths/www/
