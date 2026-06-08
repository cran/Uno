## ----setup, include = FALSE---------------------------------------------------
knitr::opts_chunk$set(
  collapse = TRUE,
  comment = "#>",
  eval = requireNamespace("Uno", quietly = TRUE)
)
library(Uno)

## ----version------------------------------------------------------------------
uno_version()

## ----maximize-----------------------------------------------------------------
# maximize -(x - 3)^2 on [0, 10]  ->  x = 3
mx <- uno_solve(
  n = 1L, lb = 0, ub = 10, sense = "maximize",
  obj  = function(x) -(x[1] - 3)^2,
  grad = function(x) -2 * (x[1] - 3),
  m = 0L, cl = numeric(), cu = numeric(), cons = function(x) numeric(),
  jac_rows = integer(), jac_cols = integer(), jac = function(x) numeric(),
  hess_rows = 1L, hess_cols = 1L, hess = function(x, sigma, lambda) sigma * (-2),
  x0 = 0, preset = "ipopt", base_indexing = 1L, verbose = FALSE,
  options = list(logger = "SILENT")
)
mx$primal

## ----quiet--------------------------------------------------------------------
quiet <- list(logger = "SILENT")

## ----hs071--------------------------------------------------------------------
hs071 <- uno_solve(
  n = 4L, lb = rep(1, 4), ub = rep(5, 4), sense = "minimize",

  obj  = function(x) x[1] * x[4] * (x[1] + x[2] + x[3]) + x[3],
  grad = function(x) c(
    x[4] * (2 * x[1] + x[2] + x[3]),
    x[1] * x[4],
    x[1] * x[4] + 1,
    x[1] * (x[1] + x[2] + x[3])
  ),

  m = 2L, cl = c(25, 40), cu = c(Inf, 40),   # g1 >= 25 ; g2 == 40
  cons = function(x) c(prod(x), sum(x^2)),

  # dense 2 x 4 Jacobian, one-based COO
  jac_rows = rep(1:2, each = 4L),
  jac_cols = rep(1:4, times = 2L),
  jac = function(x) c(
    x[2] * x[3] * x[4], x[1] * x[3] * x[4], x[1] * x[2] * x[4], x[1] * x[2] * x[3],
    2 * x[1], 2 * x[2], 2 * x[3], 2 * x[4]
  ),

  # lower triangle of the 4 x 4 Lagrangian Hessian, row by row
  hess_rows = as.integer(c(1, 2, 2, 3, 3, 3, 4, 4, 4, 4)),
  hess_cols = as.integer(c(1, 1, 2, 1, 2, 3, 1, 2, 3, 4)),
  hess = function(x, sigma, lambda) {
    l1 <- lambda[1]; l2 <- lambda[2]
    c(
      sigma * (2 * x[4])                  + l2 * 2,        # (1,1)
      sigma * x[4]            + l1 * (x[3] * x[4]),        # (2,1)
      l2 * 2,                                             # (2,2)
      sigma * x[4]            + l1 * (x[2] * x[4]),        # (3,1)
      l1 * (x[1] * x[4]),                                 # (3,2)
      l2 * 2,                                             # (3,3)
      sigma * (2 * x[1] + x[2] + x[3]) + l1 * (x[2] * x[3]),  # (4,1)
      sigma * x[1]            + l1 * (x[1] * x[3]),        # (4,2)
      sigma * x[1]            + l1 * (x[1] * x[2]),        # (4,3)
      l2 * 2                                              # (4,4)
    )
  },

  x0 = c(1, 5, 5, 1), preset = "ipopt", base_indexing = 1L, verbose = FALSE,
  options = quiet, lagrangian_sign = "positive"
)

names(hs071$optimization_status)
hs071$objective                    # 17.014
hs071$primal                       # (1, 4.743, 3.821, 1.379)
hs071$constraint_dual

## ----hs015--------------------------------------------------------------------
hs015 <- uno_solve(
  n = 2L, lb = c(-Inf, -Inf), ub = c(0.5, Inf), sense = "minimize",

  obj  = function(x) 100 * (x[2] - x[1]^2)^2 + (1 - x[1])^2,
  grad = function(x) c(
    400 * x[1]^3 - 400 * x[1] * x[2] + 2 * x[1] - 2,
    200 * (x[2] - x[1]^2)
  ),

  m = 2L, cl = c(1, 0), cu = c(Inf, Inf),
  cons = function(x) c(x[1] * x[2], x[1] + x[2]^2),

  jac_rows = as.integer(c(1, 2, 1, 2)),
  jac_cols = as.integer(c(1, 1, 2, 2)),
  jac = function(x) c(x[2], 1, x[1], 2 * x[2]),

  hess_rows = as.integer(c(1, 2, 2)),
  hess_cols = as.integer(c(1, 1, 2)),
  hess = function(x, sigma, lambda) c(
    sigma * (1200 * x[1]^2 - 400 * x[2] + 2),   # (1,1): from the objective
    -400 * sigma * x[1] - lambda[1],            # (2,1): note the minus signs
    200 * sigma - 2 * lambda[2]                 # (2,2)
  ),

  x0 = c(-2, 1), preset = "ipopt", base_indexing = 1L, verbose = FALSE,
  options = quiet                                # lagrangian_sign = "negative" (default)
)

names(hs015$optimization_status)
hs015$objective                    # 306.5
hs015$primal                       # (0.5, 2)

## ----rosenbrock---------------------------------------------------------------
rosen <- uno_solve(
  n = 2L, lb = c(-Inf, -Inf), ub = c(Inf, Inf), sense = "minimize",

  obj  = function(x) (1 - x[1])^2 + 100 * (x[2] - x[1]^2)^2,
  grad = function(x) c(
    -2 * (1 - x[1]) - 400 * x[1] * (x[2] - x[1]^2),
    200 * (x[2] - x[1]^2)
  ),

  m = 0L, cl = numeric(), cu = numeric(), cons = function(x) numeric(),
  jac_rows = integer(), jac_cols = integer(), jac = function(x) numeric(),

  hess_rows = as.integer(c(1, 2, 2)),
  hess_cols = as.integer(c(1, 1, 2)),
  hess = function(x, sigma, lambda) sigma * c(
    2 - 400 * x[2] + 1200 * x[1]^2,   # (1,1)
    -400 * x[1],                      # (2,1)
    200                               # (2,2)
  ),

  x0 = c(-1.2, 1), preset = "ipopt", base_indexing = 1L, verbose = FALSE,
  options = quiet
)
rosen$primal

## ----rosenbrock-lbfgs---------------------------------------------------------
rosen_lm <- uno_solve(
  n = 2L, lb = c(-Inf, -Inf), ub = c(Inf, Inf), sense = "minimize",
  obj  = function(x) (1 - x[1])^2 + 100 * (x[2] - x[1]^2)^2,
  grad = function(x) c(
    -2 * (1 - x[1]) - 400 * x[1] * (x[2] - x[1]^2),
    200 * (x[2] - x[1]^2)
  ),
  m = 0L, cl = numeric(), cu = numeric(), cons = function(x) numeric(),
  jac_rows = integer(), jac_cols = integer(), jac = function(x) numeric(),
  hess_rows = integer(), hess_cols = integer(), hess = NULL,
  x0 = c(-1.2, 1), preset = "ipopt", base_indexing = 1L, verbose = FALSE,
  options = quiet
)
rosen_lm$primal

## ----maxent-------------------------------------------------------------------
K <- 5L; support <- 1:K; mu <- 2

ent <- uno_solve(
  n = K, lb = rep(1e-8, K), ub = rep(1, K), sense = "minimize",   # p_i > 0

  obj  = function(p) sum(p * log(p)),
  grad = function(p) log(p) + 1,

  m = 2L, cl = c(1, mu), cu = c(1, mu),                # two equalities
  cons = function(p) c(sum(p), sum(support * p)),
  jac_rows = rep(1:2, each = K),
  jac_cols = rep(seq_len(K), times = 2L),
  jac = function(p) c(rep(1, K), support),             # constant Jacobian

  hess_rows = seq_len(K), hess_cols = seq_len(K),      # diagonal only
  hess = function(p, sigma, lambda) sigma * (1 / p),

  x0 = rep(1 / K, K), preset = "ipopt", base_indexing = 1L, verbose = FALSE,
  options = quiet
)

round(ent$primal, 5)
c(total = sum(ent$primal), mean = sum(support * ent$primal))

## ----maxent-check-------------------------------------------------------------
theta <- ent$constraint_dual[2]
cf <- exp(-theta * support); cf <- cf / sum(cf)
round(cf, 5)                       # matches ent$primal

## ----qp-filtersqp-------------------------------------------------------------
qp <- uno_solve(
  n = 2L, lb = c(0, 0), ub = c(Inf, Inf), sense = "minimize",
  obj  = function(x) (x[1] - 1)^2 + (x[2] - 2.5)^2,
  grad = function(x) c(2 * (x[1] - 1), 2 * (x[2] - 2.5)),
  m = 1L, cl = -Inf, cu = 3,
  cons = function(x) x[1] + x[2],
  jac_rows = as.integer(c(1, 1)), jac_cols = as.integer(c(1, 2)),
  jac = function(x) c(1, 1),
  hess_rows = as.integer(c(1, 2)), hess_cols = as.integer(c(1, 2)),
  hess = function(x, sigma, lambda) sigma * c(2, 2),
  x0 = c(0, 0), preset = "filtersqp", base_indexing = 1L, verbose = FALSE,
  options = quiet
)
qp$primal
qp$objective

## ----options, eval = FALSE----------------------------------------------------
# # cap the iteration count and tighten the tolerance
# uno_solve(..., preset = "ipopt",
#           options = list(max_iterations = 200L, tolerance = 1e-8))
# 
# # pick the linear solver explicitly, and silence output
# uno_solve(..., preset = "ipopt",
#           options = list(linear_solver = "MUMPS", logger = "SILENT"))

## ----itercb-monitor-----------------------------------------------------------
ros_obj  <- function(x) (1 - x[1])^2 + 100 * (x[2] - x[1]^2)^2
ros_grad <- function(x) c(-2 * (1 - x[1]) - 400 * x[1] * (x[2] - x[1]^2),
                          200 * (x[2] - x[1]^2))
ros_hess <- function(x, sigma, lambda)
  sigma * c(2 - 400 * x[2] + 1200 * x[1]^2, -400 * x[1], 200)

stationarity <- numeric(0)
mon <- uno_solve(
  n = 2L, lb = c(-Inf, -Inf), ub = c(Inf, Inf), sense = "minimize",
  obj = ros_obj, grad = ros_grad,
  m = 0L, cl = numeric(), cu = numeric(), cons = function(x) numeric(),
  jac_rows = integer(), jac_cols = integer(), jac = function(x) numeric(),
  hess_rows = as.integer(c(1, 2, 2)), hess_cols = as.integer(c(1, 1, 2)), hess = ros_hess,
  x0 = c(-1.2, 1), preset = "ipopt", base_indexing = 1L, verbose = FALSE, options = quiet,
  iter_callback = function(info) {
    stationarity[[length(stationarity) + 1L]] <<- info$stationarity
    FALSE                                            # keep going
  }
)
names(mon$optimization_status)
length(stationarity)                                 # one entry per acceptable iterate
signif(range(stationarity), 3)                       # residual shrinks toward 0

## ----itercb-stop--------------------------------------------------------------
iter <- 0L
stopped <- uno_solve(
  n = 2L, lb = c(-Inf, -Inf), ub = c(Inf, Inf), sense = "minimize",
  obj = ros_obj, grad = ros_grad,
  m = 0L, cl = numeric(), cu = numeric(), cons = function(x) numeric(),
  jac_rows = integer(), jac_cols = integer(), jac = function(x) numeric(),
  hess_rows = as.integer(c(1, 2, 2)), hess_cols = as.integer(c(1, 1, 2)), hess = ros_hess,
  x0 = c(-1.2, 1), preset = "ipopt", base_indexing = 1L, verbose = FALSE, options = quiet,
  iter_callback = function(info) { iter <<- iter + 1L; iter >= 5L }
)
iter                                 # stopped after the 5th acceptable iterate
names(stopped$optimization_status)   # "USER_TERMINATION"

