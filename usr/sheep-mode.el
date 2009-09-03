;;; sheep-mode.el
;;;
;;; Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>

;;;###autoload
(defun sheep-mode ()
  "Major mode for editing sheep files."
  (interactive)
  (kill-all-local-variables)
  (setq major-mode 'sheep-mode
	mode-name "Sheep")
  (set (make-local-variable 'indent-line-function)
       'sheep-indent-function)
  (set-syntax-table sheep-mode-syntax-table)
  (set (make-local-variable 'comment-start)
       "#")
  (set (make-local-variable 'comment-start-skip)
       "#+ *")
  (set (make-local-variable 'comment-end)
       "")
  (setq font-lock-defaults
	'((sheep-font-lock-keywords)
	  nil nil nil
	  beginning-of-defun
	  (font-lock-mark-block-function . mark-defun))))

(defun sheep-indent-function ()
  'noindent)

(defvar sheep-mode-syntax-table
  (let ((table (make-syntax-table))
	(i 0))
    (while (< i 256)
      (modify-syntax-entry i "_   " table)
      (setq i (1+ i)))
    ;; Whitespace
    (modify-syntax-entry ?\t "    " table)
    (modify-syntax-entry ?\n ">   " table)
    (modify-syntax-entry ?\f "    " table)
    (modify-syntax-entry ?\r "    " table)
    (modify-syntax-entry ?\s "    " table)
    ;; Comment
    (modify-syntax-entry ?\# "<   " table)
    ;; String
    (modify-syntax-entry ?\" "\"   " table)
    ;; Parenthesis
    (modify-syntax-entry ?\( "()  " table)
    (modify-syntax-entry ?\) ")(  " table)))

(defvar sheep-font-lock-keywords
  `(,(rx symbol-start
	 (or "quote" "block" "if" "with" "variable" "set" "function")
	 symbol-end)))

(provide 'sheep-mode)
