#!/bin/bash

mkdir -p /libraryit/ebooks
mkdir -p /libraryit/papers
mkdir -p /libraryit/docs
mkdir -p /libraryit/sourcecode
mkdir -p /logs

# =========================
# GROUP
# =========================

groupadd readonly || true
groupadd staff || true

# =========================
# USER
# =========================

id member >/dev/null 2>&1 || useradd -m member
id contributor >/dev/null 2>&1 || useradd -m contributor
id librarian >/dev/null 2>&1 || useradd -m librarian

# PASSWORD LINUX

echo 'member:member123' | chpasswd
echo 'contributor:contrib456' | chpasswd
echo 'librarian:libr789' | chpasswd

# GROUP MEMBER

usermod -aG readonly member
usermod -aG staff contributor
usermod -aG staff librarian

# =========================
# SAMBA PASSWORD
# =========================

(echo member123; echo member123) | smbpasswd -a -s member
(echo contrib456; echo contrib456) | smbpasswd -a -s contributor
(echo libr789; echo libr789) | smbpasswd -a -s librarian

# =========================
# OWNERSHIP
# =========================

chown root:staff /libraryit/ebooks
chown root:staff /libraryit/papers
chown librarian:staff /libraryit/docs
chown root:staff /libraryit/sourcecode

# =========================
# PERMISSION
# =========================

chmod 770 /libraryit/ebooks
chmod 770 /libraryit/papers
chmod 774 /libraryit/docs
chmod 750 /libraryit/sourcecode

# =========================
# LOG FILE
# =========================

touch /logs/access.log
chmod 666 /logs/access.log

# =========================
# START SAMBA
# =========================

smbd --foreground --no-process-group
