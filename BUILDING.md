# Building and installing Bazaar from latest source on Universal Blues Bazzite/Bluefin/Aurora for testing
```
git clone https://github.com/ublue-os/packages
```

```
cd packages
```

Click "Copy full SHA" [here](https://github.com/kolunmi/bazaar/commits/master/)

bump this line `staging/bazaar/bazaar.spec` at the top to the newest Git commit SHA

and replace the placeholder at the top of the file

```
# staging/bazaar/bazaar.spec
# renovate: datasource=git-refs depName=https://github.com/kolunmi/bazaar.git versioning=loose currentValue=master
%global commit put_the_commit_hash_here
```

build the thing (might take a while)

as long as fedora doesn't have the blueprint-compiler version needed you'll have to add the ublue-os/staging copr as a build argument to the mock wrapper

```
just build ./staging/bazaar/bazaar.spec -a "https://download.copr.fedorainfracloud.org/results/ublue-os/staging/fedora-$(rpm -E %fedora)-$(uname -m)/"
```


allow temporary changes to `/usr`
```
sudo bootc usroverlay
```

swap out system bazaar with latest bazaar
```
sudo dnf5 swap bazaar "mock/fedora-42-x86-64/result/*.rpm"
```

to undo the installation just reboot your system

## Find out which version is installed for bug reports
```
rpm -qi bazaar
```
## Verbose output
```
G_MESSAGES_DEBUG=all bazaar window --auto-service
```
