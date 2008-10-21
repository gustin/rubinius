def test_case
{"RawParseTree"=>
  [:defn,
   :f,
   [:scope,
    [:block,
     [:args, :mand, :opt, :"*rest", [:block, [:lasgn, :opt, [:lit, 42]]]],
     [:block_arg, :block],
     [:nil]]]],
 "Ruby"=>"def f(mand, opt = 42, *rest, &block)\n  # do nothing\nend",
 "RubyParser"=>
  s(:defn,
   :f,
   s(:args,
    :mand,
    :opt,
    :"*rest",
    :"&block",
    s(:block, s(:lasgn, :opt, s(:lit, 42)))),
   s(:scope, s(:block, s(:nil))))}
end
