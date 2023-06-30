package didagle

type Span interface {
	NewChild(name string) Span

	// SetAttribute sets Attribute with (name, value).
	SetAttribute(name string, value interface{})
	// AddEvent adds a event.
	AddEvent(name string)

	End()
}
