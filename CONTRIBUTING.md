# Contributing Guide

Thank you for contributing to Bazaar! Here are some instructions to get you started. 

* [New Contributor Guide](#contributing-guide)
  * [Ways to Contribute](#ways-to-contribute)
  * [Find an Issue](#find-an-issue)
  * [Ask for Help](#ask-for-help)
  * [Pull Request Lifecycle](#pull-request-lifecycle)
  * [Development Environment Setup](#development-environment-setup)
  * [Sign Your Commits](#sign-your-commits)
  * [Pull Request Checklist](#pull-request-checklist)

Welcome! We are glad that you want to contribute to our project! 💖

As you get started, you are in the best position to give us feedback on areas of
our project that we need help with including:

* Problems found during setting up a new developer environment
* Documentation
* Bugs in our automation scripts and actions

If anything doesn't make sense, or doesn't work when you run it, please open a
bug report and let us know!

## Ways to Contribute

We welcome many different types of contributions including:

* New features
* Builds, CI/CD
* Bug fixes
* Documentation
* Issue Triage
* Answering questions in Discussions
* Release management
* [Translations](https://github.com/kolunmi/bazaar/blob/master/TRANSLATORS.md) - follow the dedicated instructions in that document

## Find an Issue

These are the issues that need the most amount of attention and would be an effective way to get started:

- [Help Wanted issues](https://github.com/kolunmi/bazaar/issues?q=is%3Aissue%20state%3Aopen%20label%3A%22help%20wanted%22)
- [Good first issues](https://github.com/kolunmi/bazaar/issues?q=is%3Aissue%20state%3Aopen%20label%3A%22good%20first%20issue%22)) has extra information to

Sometimes there won’t be any issues with these labels. That’s ok! There is
likely still something for you to work on. If you want to contribute but you
don’t know where to start or can't find a suitable issue then feel free to post on the [discussion forum](https://github.com/kolunmi/bazaar/discussions)

Once you see an issue that you'd like to work on, please post a comment saying
that you want to work on it. Something like "I want to work on this" is fine.

## Ask for Help

The best way to reach us with a question when contributing is to ask on:

* The original github issue you want to contribute to
* The [discussions](https://github.com/kolunmi/bazaar/discussions) area

## Pull Request Lifecycle

[Instructions](https://contribute.cncf.io/maintainers/github/templates/required/contributing/#pull-request-lifecycle)

⚠️ **Explain your pull request process**

## Development Environment Setup

### Building and installing Bazaar

These instructions cover building from source on a Bazzite system. Please feel free to contribute instructions for other systems:


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

#### Find out which version is installed for bug reports
```
rpm -qi bazaar
```
#### Verbose output
```
G_MESSAGES_DEBUG=all bazaar window --auto-service
```

## Sign Your Commits

[Instructions](https://contribute.cncf.io/maintainers/github/templates/required/contributing/#sign-your-commits)

## Pull Request Checklist

When you submit your pull request, or you push new commits to it, our automated
systems will run some checks on your new code. We require that your pull request
passes these checks, but we also have more criteria than just that before we can
accept and merge it. We recommend that you check the following things locally
before you submit your code:

- [ ] Use the GNU Style Guide
- [ ] Follow the [GNOME Commit Style](https://handbook.gnome.org/development/commit-messages.html)
