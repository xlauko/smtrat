(set-logic QF_NRA)
(declare-fun a () Real)
(declare-fun h () Real)
(declare-fun R () Real)
(declare-fun b () Real)
(assert (and (not (= b 0)) (< 0 R) (< 0 b) (< 0 h) (= (+ (+ (+ (- (- (- (* 4 (* (* a a) (* (* h h) (* (* b b) (* b b))))) (* 2 (* (* a a) (* (* b b) (* (* b b) (* b b)))))) (* 8 (* (* (* a a) (* R R)) (* (* h h) (* b b))))) (* 8 (* (* R R) (* (* h h) (* (* b b) (* b b)))))) (* (* (* b b) (* b b)) (* (* b b) (* b b)))) (* (* (* a a) (* a a)) (* (* b b) (* b b)))) (* 16 (* (* (* R R) (* R R)) (* (* h h) (* h h))))) 0) (> 0 (* (- (- (* 2 (* R h)) (* a b)) (* b b)) b)) (< 0 (* R (* h b))) (< 0 (* (- (+ (* 2 (* R h)) (* a b)) (* b b)) b)) (< 0 (* (- (+ (* b b) (* 2 (* R h))) (* a b)) b))))
(eliminate-quantifiers (exists b))
(exit)