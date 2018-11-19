(set-logic QF_NRA)
(declare-fun a () Real)
(declare-fun h () Real)
(declare-fun R () Real)
(declare-fun s () Real)
(declare-fun b () Real)
(declare-fun c () Real)
(assert (and (= (- (* (* a a) (* h h)) (* 4 (* s (* (- s a) (* (- s b) (- s c)))))) 0) (= (- (* 2 (* R h)) (* b c)) 0) (= (- (- (- (* 2 s) a) b) c) 0) (> b 0) (> c 0) (> R 0) (> h 0) (> (- (+ a b) c) 0) (> (- (+ b c) a) 0) (> (- (+ a c) b) 0)))
(eliminate-quantifiers (exists s b c))
(exit)