package didagle

type Params struct {
	val    map[string]interface{}
	parent *Params
}

func NewParams(val map[string]interface{}) *Params {
	if nil == val {
		val = make(map[string]interface{})
	}
	return &Params{
		val: val,
	}
}
func (p *Params) Merge(other *Params) {
	if nil == other {
		return
	}
	for k, v := range other.val {
		if _, exist := p.val[k]; !exist {
			p.val[k] = v
		}
	}
}

func (p *Params) reset() {
	p.parent = nil
}

func (p *Params) setParent(v *Params) {
	p.parent = v
}
func (p *Params) set(name string, val interface{}) {
	p.val[name] = val
}

func (p *Params) Get(name string) (interface{}, bool) {
	if p == nil {
		return nil, false
	}
	if nil != p.val {
		val, exist := p.val[name]
		if exist {
			return val, true
		}
	}
	if nil != p.parent {
		return p.parent.Get(name)
	}
	return nil, false
}
