;;; sheep-mode.el
;;;
;;; Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>

(require 'generic)

(define-generic-mode 'sheep-mode
  ()
  (list "quote" "block" "with" "variable" "function" "if")
  ()
  ;(list "ddump" "cons" "list" "head" "tail" "map")
  (list "\\.sheep\\'")
  (list 'sheep-mode-init)
  "Major mode for editing Sheep files.")

(defun sheep-mode-init ())

(provide 'sheep-mode)
