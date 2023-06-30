package didagle

import (
	"log"
	"os"
)

// Logger defines an interface for writing log messages.
type Logger interface {
	Debugf(format string, args ...interface{})
	Infof(format string, args ...interface{})
	Errorf(format string, args ...interface{})
	Fatalf(format string, args ...interface{})
}

type defaultLogger struct{}

// DefaultLogger logs to the Go stdlib logs.
var DefaultLogger Logger = &defaultLogger{}

func (*defaultLogger) Debugf(format string, args ...interface{}) {
	log.Printf(format, args...)
}

// Infof implements the Logger.Infof interface.
func (*defaultLogger) Infof(format string, args ...interface{}) {
	log.Printf(format, args...)
}
func (*defaultLogger) Errorf(format string, args ...interface{}) {
	log.Printf(format, args...)
}

// Fatalf implements the Logger.Fatalf interface.
func (*defaultLogger) Fatalf(format string, args ...interface{}) {
	log.Printf(format, args...)
	os.Exit(1)
}
