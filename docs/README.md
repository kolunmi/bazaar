# Docs

You can view the docs as html by executing the following inside of this directory:
```sh
emacs --batch --eval "
  (with-current-buffer (find-file \"overview.org\")
    (let ((html (org-export-as 'html)))
      (with-temp-file \"overview.html\"
        (insert html))
      (browse-url-xdg-open (concat \"file://\" (file-truename \"overview.html\")))))"
```
