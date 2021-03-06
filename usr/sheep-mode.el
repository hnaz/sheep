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
  (set (make-local-variable 'indent-tabs-mode)
       nil)
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
  (let ((target
	 (save-excursion
	   (back-to-indentation)
	   (let ((state (syntax-ppss)))
	     ;; Toplevel
	     (if (zerop (car state))
		 0
	       (goto-char (1+ (nth 1 state)))
	       (if (looking-at sheep-specials)
		   ;; Block start
		   (1+ (current-column))
		 (forward-sexp)
		 (1+ (if (looking-at "\\s-*$")
			 ;; Arguments align with operator
			 (progn
			   (goto-char (nth 1 state))
			   (current-column))
		       ;; Arguments align with first argument
		       (current-column)))))))))
    (let ((offset (- (current-column) (current-indentation))))
      (indent-line-to target)
      (if (> offset 0)
	  (forward-char offset)))))

(defvar sheep-mode-syntax-table
  (let ((table (make-syntax-table))
	(i 0))
    (while (< i 256)
      (modify-syntax-entry i "_   " table)
      (modify-syntax-entry i "w   " table)
      (setq i (1+ i)))
    ;; Whitespace
    (modify-syntax-entry ?\t "    " table)
    (modify-syntax-entry ?\n ">   " table)
    (modify-syntax-entry ?\f "    " table)
    (modify-syntax-entry ?\r "    " table)
    (modify-syntax-entry ?\s "    " table)
    ;; Punctuation
    (modify-syntax-entry ?: ".   " table)
    ;; Parenthesis
    (modify-syntax-entry ?\( "()  " table)
    (modify-syntax-entry ?\) ")(  " table)
    ;; String
    (modify-syntax-entry ?\" "\"   " table)
    ;; Comment
    (modify-syntax-entry ?\# "<   " table)
    table))

(defvar sheep-specials
  (rx symbol-start
      (| "and" "block" "function" "if" "load" "or" "quote"
	 "set" "type" "variable" "with")
      symbol-end))

(defvar sheep-font-lock-keywords
  `(,sheep-specials))

;;;###autoload
(add-to-list 'auto-mode-alist '("\\.sheep\\'" . sheep-mode))

(provide 'sheep-mode)
