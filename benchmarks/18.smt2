(set-logic QF_NRA)
(declare-fun a () Real)
(declare-fun b () Real)
(declare-fun x () Real)
(declare-fun y () Real)
(assert (and (= (- (- (* 3 (*  x x)) (* 2 x)) a) 0) (= (- (+ (- (- (- (* x (* x x)) (* x x)) (* a x)) (* 2 b)) a) 2) 0) (= (- (- (* 3 (* y y)) (* 2 y)) a) 0) (= (+ (- (- (- (* y (* y y)) (* y y)) (* a y)) a) 2) 0) (<= 1 (* 4 a)) (<= (* 4 a) 7) (<= (- 0 3) (* 4 b)) (<= (* 4 b) 3) (<= (- 0 1) x) (<= x 0) (<= 0 y) (<= y 1)))
(eliminate-quantifiers (exists y x))
(exit)
